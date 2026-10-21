#include "validation.h"

#include <chrono>
#include <cctype>
#include <regex>
#include <stdexcept>

namespace
{
bool isWhitespaceOnly(const std::string &value)
{
    for (const unsigned char character : value)
    {
        if (!std::isspace(character))
        {
            return false;
        }
    }
    return true;
}

validation::Result invalid(const std::string &message)
{
    return {false, message};
}
}

namespace validation
{
Result requiredString(const std::string &value, const std::string &field)
{
    if (value.empty() || isWhitespaceOnly(value))
    {
        return invalid(field + " is required");
    }
    return {true, {}};
}

Result positiveInteger(const std::string &value, const std::string &field)
{
    if (value.empty())
    {
        return invalid(field + " is required");
    }

    try
    {
        std::size_t parsed_characters = 0;
        const long long number = std::stoll(value, &parsed_characters);
        if (parsed_characters != value.size() || number <= 0)
        {
            return invalid(field + " must be a positive integer");
        }
    }
    catch (const std::exception &)
    {
        return invalid(field + " must be a positive integer");
    }

    return {true, {}};
}

Result email(const std::string &value)
{
    static const std::regex pattern(
        R"(^[A-Za-z0-9.!#$%&'*+/=?^_`{|}~-]+@[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)+$)");

    if (!std::regex_match(value, pattern))
    {
        return invalid("email must be a valid email address");
    }
    return {true, {}};
}

Result allowedValue(
    const std::string &value,
    const std::string &field,
    std::initializer_list<const char *> allowed_values)
{
    for (const char *allowed : allowed_values)
    {
        if (value == allowed)
        {
            return {true, {}};
        }
    }

    return invalid(field + " has an unsupported value");
}

Result timestamp(const std::string &value, const std::string &field)
{
    static const std::regex pattern(
        R"(^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(\.\d{1,9})?(Z|([+-])(\d{2}):(\d{2}))$)");

    std::smatch matches;
    if (!std::regex_match(value, matches, pattern))
    {
        return invalid(field + " must be an ISO-8601 timestamp");
    }

    const int year = std::stoi(matches[1].str());
    const unsigned int month = static_cast<unsigned int>(std::stoi(matches[2].str()));
    const unsigned int day = static_cast<unsigned int>(std::stoi(matches[3].str()));
    const int hour = std::stoi(matches[4].str());
    const int minute = std::stoi(matches[5].str());
    const int second = std::stoi(matches[6].str());
    const std::chrono::year_month_day date{
        std::chrono::year{year},
        std::chrono::month{month},
        std::chrono::day{day}};
    if (year < 1 || !date.ok() || hour > 23 || minute > 59 || second > 59)
    {
        return invalid(field + " must be a valid ISO-8601 timestamp");
    }

    if (matches[8].str() != "Z")
    {
        const int offset_hour = std::stoi(matches[10].str());
        const int offset_minute = std::stoi(matches[11].str());
        if (offset_hour > 14 || offset_minute > 59 ||
            (offset_hour == 14 && offset_minute != 0))
        {
            return invalid(field + " must have a valid ISO-8601 timezone offset");
        }
    }
    return {true, {}};
}
}
