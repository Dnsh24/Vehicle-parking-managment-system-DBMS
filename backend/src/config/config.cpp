#include "config.h"

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace
{
std::string getEnvironmentValue(const char *name, const char *default_value = nullptr)
{
    const char *value = std::getenv(name);
    if (value != nullptr && value[0] != '\0')
    {
        return value;
    }

    if (default_value != nullptr)
    {
        return default_value;
    }

    throw std::runtime_error(std::string("Required environment variable is missing: ") + name);
}

unsigned short getPort(const char *name, const char *default_value)
{
    const std::string value = getEnvironmentValue(name, default_value);

    try
    {
        std::size_t parsed_characters = 0;
        const unsigned long port = std::stoul(value, &parsed_characters);
        if (parsed_characters != value.size())
        {
            throw std::invalid_argument("port contains non-numeric characters");
        }
        if (port == 0 || port > 65535)
        {
            throw std::out_of_range("port outside valid range");
        }
        return static_cast<unsigned short>(port);
    }
    catch (const std::exception &)
    {
        throw std::runtime_error(std::string("Invalid port in environment variable: ") + name);
    }
}
}

BackendConfig BackendConfig::fromEnvironment()
{
    return {
        getEnvironmentValue("PARKING_DB_HOST", "localhost"),
        getEnvironmentValue("PARKING_DB_PORT", "5432"),
        getEnvironmentValue("PARKING_DB_NAME", "vehicle_parking"),
        getEnvironmentValue("PARKING_DB_USER", "postgres"),
        getEnvironmentValue("PARKING_DB_PASSWORD"),
        getPort("PARKING_SERVER_PORT", "3000")};
}

