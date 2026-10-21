#include "route_helpers.h"
#include "auth/auth.h"

#include <algorithm>
#include <cctype>

namespace routes
{

std::string lowerEmail(std::string email)
{
    std::transform(email.begin(), email.end(), email.begin(), [](unsigned char character)
                   { return static_cast<char>(std::tolower(character)); });
    return email;
}

std::string normalizeRegistration(std::string registration)
{
    const auto first = registration.find_first_not_of(" \t\n\r\f\v");
    if (first == std::string::npos)
    {
        return {};
    }
    const auto last = registration.find_last_not_of(" \t\n\r\f\v");
    registration = registration.substr(first, last - first + 1);
    std::transform(registration.begin(), registration.end(), registration.begin(),
                   [](unsigned char character)
                   { return static_cast<char>(std::toupper(character)); });
    return registration;
}

std::string jsonString(const crow::json::rvalue &body, const char *key)
{
    if (body.t() != crow::json::type::Object ||
        !body.has(key) ||
        body[key].t() != crow::json::type::String)
    {
        return {};
    }
    return body[key].s();
}

std::optional<bool> jsonBoolean(const crow::json::rvalue &body, const char *key)
{
    if (body.t() != crow::json::type::Object || !body.has(key))
    {
        return false;
    }
    if (body[key].t() != crow::json::type::True &&
        body[key].t() != crow::json::type::False)
    {
        return std::nullopt;
    }
    return body[key].t() == crow::json::type::True;
}

std::optional<std::int64_t> jsonInteger(const crow::json::rvalue &body, const char *key)
{
    if (body.t() != crow::json::type::Object || !body.has(key))
    {
        return std::nullopt;
    }
    if (body[key].t() != crow::json::type::Number)
    {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(body[key].i());
}

std::optional<double> jsonNumber(const crow::json::rvalue &body, const char *key)
{
    if (body.t() != crow::json::type::Object || !body.has(key))
    {
        return std::nullopt;
    }
    if (body[key].t() != crow::json::type::Number)
    {
        return std::nullopt;
    }
    return body[key].d();
}

crow::response unauthorized()
{
    return api::errorResponse(401, "UNAUTHENTICATED", "A valid login session is required.");
}

crow::response forbidden()
{
    return api::errorResponse(403, "FORBIDDEN", "Administrator access is required.");
}

crow::json::wvalue userJson(const AuthenticatedUser &user)
{
    crow::json::wvalue response;
    response["user_id"] = user.user_id;
    response["name"] = user.name;
    response["email"] = user.email;
    response["phone"] = user.phone;
    response["role"] = user.role;
    return response;
}

std::optional<AuthenticatedUser> findAuthenticatedUser(
    const Database &database,
    const crow::request &request)
{
    const std::string authorization = request.get_header_value("Authorization");
    constexpr const char *prefix = "Bearer ";
    if (authorization.rfind(prefix, 0) != 0 || authorization.size() <= 7)
    {
        return std::nullopt;
    }

    const pqxx::result rows = database.executeRead(
        "SELECT u.user_id, u.name, u.email, COALESCE(u.phone, '') AS phone, u.role "
        "FROM auth_sessions s "
        "JOIN users u ON u.user_id = s.user_id "
        "WHERE s.token_hash = $1 AND s.expires_at > CURRENT_TIMESTAMP "
        "AND u.is_active = TRUE",
        pqxx::params{auth::hashToken(authorization.substr(7))});
    if (rows.empty())
    {
        return std::nullopt;
    }

    return AuthenticatedUser{
        rows[0]["user_id"].as<std::int64_t>(),
        rows[0]["name"].c_str(),
        rows[0]["email"].c_str(),
        rows[0]["phone"].c_str(),
        rows[0]["role"].c_str()};
}

std::optional<AuthenticatedUser> findAuthenticatedAdmin(
    const Database &database,
    const crow::request &request)
{
    const auto user = findAuthenticatedUser(database, request);
    if (!user || user->role != "admin")
    {
        return std::nullopt;
    }
    return user;
}

std::string createSession(const Database &database, std::int64_t user_id)
{
    const std::string token = auth::createToken();
    database.executeWrite(
        "DELETE FROM auth_sessions WHERE expires_at <= CURRENT_TIMESTAMP",
        pqxx::params{});
    database.executeWrite(
        "INSERT INTO auth_sessions (user_id, token_hash, expires_at) "
        "VALUES ($1, $2, CURRENT_TIMESTAMP + INTERVAL '7 days')",
        pqxx::params{user_id, auth::hashToken(token)});
    return token;
}

} // namespace routes

