#include "auth_routes.h"
#include "route_helpers.h"
#include "auth/auth.h"
#include "api/errors.h"
#include "validation/validation.h"

#include <iostream>

namespace routes
{

void registerAuthRoutes(crow::App<crow::CORSHandler> &app, Database &database)
{
    CROW_ROUTE(app, "/api/auth/register").methods(crow::HTTPMethod::POST)
    ([&database](const crow::request &request)
     {
        try
        {
            const auto body = crow::json::load(request.body);
            const std::string username = jsonString(body, "username");
            const std::string provided_name = jsonString(body, "name");
            const std::string name = provided_name.empty() ? username : provided_name;
            const std::string email = lowerEmail(jsonString(body, "email"));
            const std::string password = jsonString(body, "password");
            const std::string phone = jsonString(body, "phone");
            if (!validation::requiredString(username, "username").valid ||
                username.size() > 50)
            {
                return api::errorResponse(400, "INVALID_USERNAME", "Username is required and must be at most 50 characters.");
            }
            if (!validation::email(email).valid)
            {
                return api::errorResponse(400, "INVALID_EMAIL", "Email must be a valid email address.");
            }
            if (password.size() < 8)
            {
                return api::errorResponse(400, "INVALID_PASSWORD", "Password must be at least 8 characters.");
            }
            const pqxx::result rows = database.executeWrite(
                "INSERT INTO users (username, name, email, password_hash, phone) "
                "VALUES ($1, $2, $3, $4, NULLIF($5, '')) "
                "RETURNING user_id, name, email, COALESCE(phone, '') AS phone, role",
                pqxx::params{username, name, email, auth::hashPassword(password), phone});
            const AuthenticatedUser user{
                rows[0]["user_id"].as<std::int64_t>(),
                rows[0]["name"].c_str(),
                rows[0]["email"].c_str(),
                rows[0]["phone"].c_str(),
                rows[0]["role"].c_str()};
            crow::json::wvalue response;
            response["user"] = userJson(user);
            response["token"] = createSession(database, user.user_id);
            return crow::response(201, response);
        }
        catch (const pqxx::unique_violation &)
        {
            return api::errorResponse(409, "ACCOUNT_IDENTIFIER_ALREADY_REGISTERED", "That username or email is already registered.");
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Registration failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The account could not be created.");
        } });

    CROW_ROUTE(app, "/api/auth/login").methods(crow::HTTPMethod::POST)
    ([&database](const crow::request &request)
     {
        try
        {
            const auto body = crow::json::load(request.body);
            const std::string email = lowerEmail(jsonString(body, "email"));
            const std::string password = jsonString(body, "password");
            const pqxx::result rows = database.executeRead(
                "SELECT user_id, name, email, password_hash, COALESCE(phone, '') AS phone, role, is_active "
                "FROM users WHERE LOWER(email) = LOWER($1) OR LOWER(username) = LOWER($1)",
                pqxx::params{email});
            if (rows.empty() ||
                !rows[0]["is_active"].as<bool>() ||
                !auth::verifyPassword(password, rows[0]["password_hash"].c_str()))
            {
                return api::errorResponse(401, "INVALID_CREDENTIALS", "Email or password is incorrect.");
            }
            const AuthenticatedUser user{
                rows[0]["user_id"].as<std::int64_t>(),
                rows[0]["name"].c_str(),
                rows[0]["email"].c_str(),
                rows[0]["phone"].c_str(),
                rows[0]["role"].c_str()};
            crow::json::wvalue response;
            response["user"] = userJson(user);
            response["token"] = createSession(database, user.user_id);
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Login failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "Login is temporarily unavailable.");
        } });

    CROW_ROUTE(app, "/api/auth/logout").methods(crow::HTTPMethod::POST)
    ([&database](const crow::request &request)
     {
        try
        {
            const std::string authorization = request.get_header_value("Authorization");
            if (authorization.rfind("Bearer ", 0) == 0)
            {
                database.executeWrite(
                    "DELETE FROM auth_sessions WHERE token_hash = $1",
                    pqxx::params{auth::hashToken(authorization.substr(7))});
            }
            return crow::response(204);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Logout failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "Logout is temporarily unavailable.");
        } });

    CROW_ROUTE(app, "/api/auth/me")
    ([&database](const crow::request &request)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }
            crow::json::wvalue response;
            response["user"] = userJson(*user);
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Session lookup failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The session could not be checked.");
        } });

    CROW_ROUTE(app, "/api/profile").methods(crow::HTTPMethod::GET)
    ([&database](const crow::request &request)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }
            crow::json::wvalue response;
            response["user"] = userJson(*user);
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Profile lookup failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The profile could not be loaded.");
        } });

    CROW_ROUTE(app, "/api/profile").methods(crow::HTTPMethod::PUT)
    ([&database](const crow::request &request)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }
            const auto body = crow::json::load(request.body);
            const std::string name = jsonString(body, "name");
            const std::string phone = jsonString(body, "phone");
            if (!validation::requiredString(name, "name").valid || name.size() > 100)
            {
                return api::errorResponse(400, "INVALID_NAME", "Name is required and must be at most 100 characters.");
            }
            const pqxx::result rows = database.executeWrite(
                "UPDATE users SET name = $1, phone = NULLIF($2, '') "
                "WHERE user_id = $3 "
                "RETURNING user_id, name, email, COALESCE(phone, '') AS phone, role",
                pqxx::params{name, phone, user->user_id});
            const AuthenticatedUser updated{
                rows[0]["user_id"].as<std::int64_t>(),
                rows[0]["name"].c_str(),
                rows[0]["email"].c_str(),
                rows[0]["phone"].c_str(),
                rows[0]["role"].c_str()};
            crow::json::wvalue response;
            response["user"] = userJson(updated);
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Profile update failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The profile could not be updated.");
        } });

    CROW_ROUTE(app, "/api/profile/password").methods(crow::HTTPMethod::PUT)
    ([&database](const crow::request &request)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }
            const auto body = crow::json::load(request.body);
            const std::string current_password = jsonString(body, "current_password");
            const std::string new_password = jsonString(body, "new_password");
            const pqxx::result rows = database.executeRead(
                "SELECT password_hash FROM users WHERE user_id = $1",
                pqxx::params{user->user_id});
            if (rows.empty() ||
                !auth::verifyPassword(current_password, rows[0]["password_hash"].c_str()))
            {
                return api::errorResponse(401, "INVALID_CURRENT_PASSWORD", "The current password is incorrect.");
            }
            if (new_password.size() < 8)
            {
                return api::errorResponse(400, "INVALID_PASSWORD", "New password must be at least 8 characters.");
            }
            database.executeWrite(
                "UPDATE users SET password_hash = $1 WHERE user_id = $2",
                pqxx::params{auth::hashPassword(new_password), user->user_id});
            return crow::response(204);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Password update failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The password could not be updated.");
        } });
}

} // namespace routes

