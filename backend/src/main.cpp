#include "crow.h"
#include "crow/middlewares/cors.h"
#include "api/errors.h"
#include "api/parking_responses.h"
#include "api/vehicle_responses.h"
#include "auth/auth.h"
#include "config/config.h"
#include "database/database.h"
#include "validation/validation.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>

#ifdef DELETE
#undef DELETE
#endif

namespace
{
struct AuthenticatedUser
{
    std::int64_t user_id;
    std::string name;
    std::string email;
    std::string phone;
    std::string role;
};

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

crow::response unauthorized()
{
    return api::errorResponse(401, "UNAUTHENTICATED", "A valid login session is required.");
}

}

int main()
{
    BackendConfig config;
    try
    {
        config = BackendConfig::fromEnvironment();
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Configuration error: " << exception.what() << std::endl;
        return 1;
    }

    crow::App<crow::CORSHandler> app;

    auto &cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .headers("Content-Type, Accept, Origin, Authorization")
        .methods(
            crow::HTTPMethod::GET,
            crow::HTTPMethod::POST,
            crow::HTTPMethod::PUT,
            crow::HTTPMethod::DELETE,
            crow::HTTPMethod::OPTIONS)
        .origin("*");

    Database database(config);
    const DatabaseCheckResult database_check = database.checkConnection();

    if (database_check.connected)
    {
        std::cout << "Database check successful!" << std::endl;
    }
    else
    {
        std::cerr << "Database connection failed: "
                  << database_check.error_message << std::endl;
    }

    CROW_ROUTE(app, "/api/hello")
    ([]()
     {
        crow::json::wvalue response;
        response["message"] = "C++ backend is working";
        return response; });

    CROW_ROUTE(app, "/api/database")
    ([database_check, database_name = config.database_name]()
     {
        if (!database_check.connected)
        {
            return api::errorResponse(
                503,
                "DATABASE_UNAVAILABLE",
                "PostgreSQL is unavailable. Check the backend logs.");
        }

        crow::json::wvalue response;
        response["status"] = "success";
        response["message"] = "C++ backend connected to PostgreSQL";
        response["database"] = database_name;
        return crow::response(response); });

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

    CROW_ROUTE(app, "/api/vehicles").methods(crow::HTTPMethod::GET)
    ([&database](const crow::request &request)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }
            const pqxx::result rows = database.executeRead(
                "SELECT v.vehicle_id, v.registration_number, v.vehicle_type, v.is_electric, "
                "a.assignment_id, a.parked_at, a.slot_id, a.slot_number, a.area_name "
                "FROM vehicles v "
                "LEFT JOIN LATERAL ("
                "SELECT active.assignment_id, active.parked_at, active.slot_id, "
                "active.slot_number, active.area_name "
                "FROM ("
                "SELECT a.assignment_id, a.parked_at, ps.slot_id, ps.slot_number, pa.name AS area_name "
                "FROM parking_assignments a "
                "JOIN parking_slots ps ON ps.slot_id = a.slot_id "
                "JOIN parking_areas pa ON pa.area_id = ps.area_id "
                "WHERE a.vehicle_id = v.vehicle_id AND a.checkout_at IS NULL "
                "UNION ALL "
                "SELECT NULL::BIGINT, s.entry_time, ps.slot_id, ps.slot_number, pa.name "
                "FROM parking_sessions s "
                "JOIN parking_slots ps ON ps.slot_id = s.slot_id "
                "JOIN parking_areas pa ON pa.area_id = ps.area_id "
                "WHERE s.vehicle_id = v.vehicle_id AND s.exit_time IS NULL "
                "AND s.status IN ('active', 'overstayed')"
                ") active ORDER BY active.parked_at DESC LIMIT 1"
                ") a ON TRUE "
                "WHERE v.user_id = $1 ORDER BY v.vehicle_id",
                pqxx::params{user->user_id});
            crow::json::wvalue response;
            response["vehicles"] = crow::json::wvalue::list();
            std::size_t index = 0;
            for (const auto &row : rows)
            {
                crow::json::wvalue vehicle = api::vehicleJson(
                    row["vehicle_id"].as<std::int64_t>(),
                    row["registration_number"].c_str(),
                    row["vehicle_type"].c_str(),
                    row["is_electric"].as<bool>());
                vehicle["can_delete"] = row["slot_id"].is_null();
                if (!row["slot_id"].is_null())
                {
                    if (!row["assignment_id"].is_null())
                    {
                        vehicle["parking"]["assignment_id"] = row["assignment_id"].as<std::int64_t>();
                    }
                    vehicle["parking"]["slot_id"] = row["slot_id"].as<std::int64_t>();
                    vehicle["parking"]["slot_number"] = row["slot_number"].c_str();
                    vehicle["parking"]["area_name"] = row["area_name"].c_str();
                    vehicle["parking"]["parked_at"] = row["parked_at"].c_str();
                }
                response["vehicles"][index++] = std::move(vehicle);
            }
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Vehicle list failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "Vehicles could not be loaded.");
        } });

    CROW_ROUTE(app, "/api/vehicles/<uint>/available-slots")
    ([&database](const crow::request &request, unsigned int vehicle_id)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }
            const pqxx::result vehicle_rows = database.executeRead(
                "SELECT vehicle_type, is_electric FROM vehicles WHERE vehicle_id = $1 AND user_id = $2",
                pqxx::params{vehicle_id, user->user_id});
            if (vehicle_rows.empty())
            {
                return api::errorResponse(404, "VEHICLE_NOT_FOUND", "The requested vehicle does not exist.");
            }
            const pqxx::result active_rows = database.executeRead(
                "SELECT assignment_id FROM parking_assignments "
                "WHERE vehicle_id = $1 AND user_id = $2 AND checkout_at IS NULL",
                pqxx::params{vehicle_id, user->user_id});
            const pqxx::result active_session_rows = database.executeRead(
                "SELECT session_id FROM parking_sessions "
                "WHERE vehicle_id = $1 AND user_id = $2 AND exit_time IS NULL "
                "AND status IN ('active', 'overstayed')",
                pqxx::params{vehicle_id, user->user_id});
            if (!active_rows.empty() || !active_session_rows.empty())
            {
                return api::errorResponse(409, "VEHICLE_ALREADY_PARKED", "Check out this vehicle before choosing another slot.");
            }

            const pqxx::result rows = database.executeRead(
                "SELECT ps.slot_id, ps.area_id, ps.slot_number, ps.vehicle_type, "
                "ps.slot_type, ps.is_ev, 'available' AS status, pa.name AS area_name "
                "FROM parking_slots ps "
                "JOIN parking_areas pa ON pa.area_id = ps.area_id "
                "WHERE (ps.status = 'available' OR ps.status IN ('occupied', 'reserved')) "
                "AND pa.status = 'active' "
                "AND ps.vehicle_type = $1 "
                "AND (NOT ps.is_ev OR $3) "
                "AND NOT EXISTS (SELECT 1 FROM parking_assignments a "
                "WHERE a.slot_id = ps.slot_id AND a.checkout_at IS NULL) "
                "AND NOT EXISTS (SELECT 1 FROM parking_assignments a "
                "WHERE a.vehicle_id = $2 AND a.checkout_at IS NULL) "
                "AND NOT EXISTS (SELECT 1 FROM parking_sessions s "
                "WHERE s.vehicle_id = $2 AND s.exit_time IS NULL "
                "AND s.status IN ('active', 'overstayed')) "
                "AND NOT EXISTS (SELECT 1 FROM reservations r "
                "WHERE r.slot_id = ps.slot_id AND r.status IN ('pending', 'confirmed', 'active') "
                "AND r.start_time <= CURRENT_TIMESTAMP AND r.end_time > CURRENT_TIMESTAMP) "
                "AND NOT EXISTS (SELECT 1 FROM parking_sessions s "
                "WHERE s.slot_id = ps.slot_id AND s.exit_time IS NULL "
                "AND s.status IN ('active', 'overstayed')) "
                "ORDER BY pa.area_id, ps.slot_number",
                pqxx::params{
                    vehicle_rows[0]["vehicle_type"].c_str(),
                    vehicle_id,
                    vehicle_rows[0]["is_electric"].as<bool>()});

            crow::json::wvalue response;
            response["slots"] = crow::json::wvalue::list();
            std::size_t index = 0;
            for (const auto &row : rows)
            {
                crow::json::wvalue slot = api::slotJson(
                    row["slot_id"].as<std::int64_t>(),
                    row["area_id"].as<std::int64_t>(),
                    row["slot_number"].c_str(),
                    row["vehicle_type"].c_str(),
                    row["slot_type"].c_str(),
                    row["is_ev"].as<bool>(),
                    row["status"].c_str());
                slot["area_name"] = row["area_name"].c_str();
                response["slots"][index++] = std::move(slot);
            }
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Available vehicle slots query failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "Available slots could not be loaded.");
        } });

    CROW_ROUTE(app, "/api/vehicles/<uint>/park").methods(crow::HTTPMethod::POST)
    ([&database](const crow::request &request, unsigned int vehicle_id)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }
            const auto body = crow::json::load(request.body);
            if (body.t() != crow::json::type::Object ||
                !body.has("slot_id") ||
                body["slot_id"].t() != crow::json::type::Number ||
                (body["slot_id"].nt() != crow::json::num_type::Signed_integer &&
                 body["slot_id"].nt() != crow::json::num_type::Unsigned_integer) ||
                body["slot_id"].i() <= 0)
            {
                return api::errorResponse(400, "INVALID_SLOT_ID", "slot_id must be a positive integer.");
            }
            const std::int64_t slot_id = body["slot_id"].i();
            const pqxx::result vehicle_rows = database.executeRead(
                "SELECT vehicle_id FROM vehicles WHERE vehicle_id = $1 AND user_id = $2",
                pqxx::params{vehicle_id, user->user_id});
            if (vehicle_rows.empty())
            {
                return api::errorResponse(404, "VEHICLE_NOT_FOUND", "The requested vehicle does not exist.");
            }

            const pqxx::result rows = database.executeWrite(
                "WITH claimed AS ("
                "UPDATE parking_slots ps SET status = 'occupied' "
                "FROM vehicles v, parking_areas pa "
                "WHERE ps.slot_id = $1 "
                "AND (ps.status = 'available' OR ps.status IN ('occupied', 'reserved')) "
                "AND v.vehicle_id = $2 AND v.user_id = $3 "
                "AND ps.vehicle_type = v.vehicle_type "
                "AND (NOT ps.is_ev OR v.is_electric) "
                "AND pa.area_id = ps.area_id AND pa.status = 'active' "
                "AND NOT EXISTS (SELECT 1 FROM reservations r "
                "WHERE r.slot_id = ps.slot_id AND r.status IN ('pending', 'confirmed', 'active') "
                "AND r.start_time <= CURRENT_TIMESTAMP AND r.end_time > CURRENT_TIMESTAMP) "
                "AND NOT EXISTS (SELECT 1 FROM parking_sessions s "
                "WHERE s.slot_id = ps.slot_id AND s.exit_time IS NULL "
                "AND s.status IN ('active', 'overstayed')) "
                "AND NOT EXISTS (SELECT 1 FROM parking_assignments a "
                "WHERE a.vehicle_id = v.vehicle_id AND a.checkout_at IS NULL) "
                "AND NOT EXISTS (SELECT 1 FROM parking_sessions s "
                "WHERE s.vehicle_id = v.vehicle_id AND s.exit_time IS NULL "
                "AND s.status IN ('active', 'overstayed')) "
                "RETURNING ps.slot_id"
                "), assigned AS ("
                "INSERT INTO parking_assignments (user_id, vehicle_id, slot_id) "
                "SELECT $3, $2, slot_id FROM claimed "
                "RETURNING assignment_id, vehicle_id, slot_id, parked_at"
                ") "
                "SELECT assigned.assignment_id, assigned.vehicle_id, assigned.slot_id, "
                "assigned.parked_at, ps.slot_number, ps.area_id, pa.name AS area_name "
                "FROM assigned "
                "JOIN parking_slots ps ON ps.slot_id = assigned.slot_id "
                "JOIN parking_areas pa ON pa.area_id = ps.area_id",
                pqxx::params{slot_id, vehicle_id, user->user_id});
            if (rows.empty())
            {
                return api::errorResponse(409, "PARKING_SLOT_UNAVAILABLE", "The slot is no longer available or is incompatible with this vehicle.");
            }

            crow::json::wvalue response;
            response["parking"]["assignment_id"] = rows[0]["assignment_id"].as<std::int64_t>();
            response["parking"]["vehicle_id"] = rows[0]["vehicle_id"].as<std::int64_t>();
            response["parking"]["slot_id"] = rows[0]["slot_id"].as<std::int64_t>();
            response["parking"]["area_id"] = rows[0]["area_id"].as<std::int64_t>();
            response["parking"]["slot_number"] = rows[0]["slot_number"].c_str();
            response["parking"]["area_name"] = rows[0]["area_name"].c_str();
            response["parking"]["parked_at"] = rows[0]["parked_at"].c_str();
            return crow::response(201, response);
        }
        catch (const pqxx::unique_violation &)
        {
            return api::errorResponse(409, "VEHICLE_OR_SLOT_ALREADY_PARKED", "This vehicle or parking slot already has an active assignment.");
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Vehicle parking assignment failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The vehicle could not be parked.");
        } });

    CROW_ROUTE(app, "/api/vehicles/<uint>/checkout").methods(crow::HTTPMethod::POST)
    ([&database](const crow::request &request, unsigned int vehicle_id)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }
            const pqxx::result rows = database.executeWrite(
                "WITH completed AS ("
                "UPDATE parking_assignments SET checkout_at = CURRENT_TIMESTAMP "
                "WHERE vehicle_id = $1 AND user_id = $2 AND checkout_at IS NULL "
                "RETURNING slot_id, checkout_at"
                "), freed AS ("
                "UPDATE parking_slots ps SET status = CASE "
                "WHEN EXISTS (SELECT 1 FROM parking_sessions s "
                "WHERE s.slot_id = ps.slot_id AND s.exit_time IS NULL "
                "AND s.status IN ('active', 'overstayed')) THEN 'occupied' "
                "WHEN EXISTS (SELECT 1 FROM reservations r "
                "WHERE r.slot_id = ps.slot_id AND r.status IN ('pending', 'confirmed', 'active') "
                "AND r.start_time <= CURRENT_TIMESTAMP AND r.end_time > CURRENT_TIMESTAMP) THEN 'reserved' "
                "ELSE 'available' END "
                "FROM completed c WHERE ps.slot_id = c.slot_id AND ps.status = 'occupied' "
                "RETURNING ps.slot_id"
                ") "
                "SELECT c.slot_id, c.checkout_at, ps.area_id "
                "FROM completed c JOIN parking_slots ps ON ps.slot_id = c.slot_id",
                pqxx::params{vehicle_id, user->user_id});
            if (rows.empty())
            {
                return api::errorResponse(404, "ACTIVE_PARKING_NOT_FOUND", "This vehicle has no active parking assignment.");
            }
            crow::json::wvalue response;
            response["message"] = "Vehicle checked out and slot released.";
            response["slot_id"] = rows[0]["slot_id"].as<std::int64_t>();
            response["area_id"] = rows[0]["area_id"].as<std::int64_t>();
            response["checkout_at"] = rows[0]["checkout_at"].c_str();
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Vehicle checkout failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The vehicle could not be checked out.");
        } });

    CROW_ROUTE(app, "/api/vehicles").methods(crow::HTTPMethod::POST)
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
            const std::string registration = normalizeRegistration(jsonString(body, "registration_number"));
            const std::string vehicle_type = jsonString(body, "vehicle_type");
            const auto is_electric = jsonBoolean(body, "is_electric");
            if (registration.empty() || registration.size() > 20)
            {
                return api::errorResponse(400, "INVALID_REGISTRATION_NUMBER", "Registration number is required and must be at most 20 characters.");
            }
            if (!validation::allowedValue(vehicle_type, "vehicle_type", {"car", "bike", "scooter", "truck", "bus", "other"}).valid)
            {
                return api::errorResponse(400, "INVALID_VEHICLE_TYPE", "Vehicle type must be car, bike, scooter, truck, bus, or other.");
            }
            if (!is_electric)
            {
                return api::errorResponse(400, "INVALID_ELECTRIC_FLAG", "is_electric must be a boolean.");
            }

            const pqxx::result rows = database.executeWrite(
                "INSERT INTO vehicles (user_id, registration_number, vehicle_type, is_electric) "
                "VALUES ($1, $2, $3, $4) "
                "RETURNING vehicle_id, registration_number, vehicle_type, is_electric",
                pqxx::params{user->user_id, registration, vehicle_type, *is_electric});
            crow::json::wvalue response;
            response["vehicle"] = api::vehicleJson(
                rows[0]["vehicle_id"].as<std::int64_t>(),
                rows[0]["registration_number"].c_str(),
                rows[0]["vehicle_type"].c_str(),
                rows[0]["is_electric"].as<bool>());
            return crow::response(201, response);
        }
        catch (const pqxx::unique_violation &)
        {
            return api::errorResponse(409, "VEHICLE_REGISTRATION_ALREADY_EXISTS", "That registration number is already registered.");
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Vehicle creation failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The vehicle could not be created.");
        } });

    CROW_ROUTE(app, "/api/vehicles/<uint>").methods(crow::HTTPMethod::PUT)
    ([&database](const crow::request &request, unsigned int vehicle_id)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }
            const auto body = crow::json::load(request.body);
            const std::string registration = normalizeRegistration(jsonString(body, "registration_number"));
            const std::string vehicle_type = jsonString(body, "vehicle_type");
            const auto is_electric = jsonBoolean(body, "is_electric");
            if (registration.empty() || registration.size() > 20)
            {
                return api::errorResponse(400, "INVALID_REGISTRATION_NUMBER", "Registration number is required and must be at most 20 characters.");
            }
            if (!validation::allowedValue(vehicle_type, "vehicle_type", {"car", "bike", "scooter", "truck", "bus", "other"}).valid)
            {
                return api::errorResponse(400, "INVALID_VEHICLE_TYPE", "Vehicle type must be car, bike, scooter, truck, bus, or other.");
            }
            if (!is_electric)
            {
                return api::errorResponse(400, "INVALID_ELECTRIC_FLAG", "is_electric must be a boolean.");
            }

            const pqxx::result rows = database.executeWrite(
                "UPDATE vehicles SET registration_number = $1, vehicle_type = $2, is_electric = $3 "
                "WHERE vehicle_id = $4 AND user_id = $5 "
                "AND NOT EXISTS (SELECT 1 FROM parking_assignments a "
                "WHERE a.vehicle_id = vehicles.vehicle_id AND a.checkout_at IS NULL) "
                "AND NOT EXISTS (SELECT 1 FROM parking_sessions s "
                "WHERE s.vehicle_id = vehicles.vehicle_id AND s.exit_time IS NULL "
                "AND s.status IN ('active', 'overstayed')) "
                "RETURNING vehicle_id, registration_number, vehicle_type, is_electric",
                pqxx::params{registration, vehicle_type, *is_electric, vehicle_id, user->user_id});
            if (rows.empty())
            {
                const pqxx::result existing = database.executeRead(
                    "SELECT EXISTS (SELECT 1 FROM vehicles WHERE vehicle_id = $1 AND user_id = $2) AS owned, "
                    "EXISTS (SELECT 1 FROM parking_assignments WHERE vehicle_id = $1 AND checkout_at IS NULL) "
                    "OR EXISTS (SELECT 1 FROM parking_sessions WHERE vehicle_id = $1 AND exit_time IS NULL "
                    "AND status IN ('active', 'overstayed')) AS parked",
                    pqxx::params{vehicle_id, user->user_id});
                if (!existing[0]["owned"].as<bool>())
                {
                    return api::errorResponse(404, "VEHICLE_NOT_FOUND", "The requested vehicle does not exist.");
                }
                if (existing[0]["parked"].as<bool>())
                {
                    return api::errorResponse(409, "VEHICLE_CURRENTLY_PARKED", "Check out this vehicle before editing it.");
                }
                return api::errorResponse(404, "VEHICLE_NOT_FOUND", "The requested vehicle could not be updated.");
            }
            crow::json::wvalue response;
            response["vehicle"] = api::vehicleJson(
                rows[0]["vehicle_id"].as<std::int64_t>(),
                rows[0]["registration_number"].c_str(),
                rows[0]["vehicle_type"].c_str(),
                rows[0]["is_electric"].as<bool>());
            return crow::response(response);
        }
        catch (const pqxx::unique_violation &)
        {
            return api::errorResponse(409, "VEHICLE_REGISTRATION_ALREADY_EXISTS", "That registration number is already registered.");
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Vehicle update failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The vehicle could not be updated.");
        } });

    CROW_ROUTE(app, "/api/vehicles/<uint>").methods(crow::HTTPMethod::DELETE)
    ([&database](const crow::request &request, unsigned int vehicle_id)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }
            const VehicleDeletionResult deletion =
                database.deleteVehicleAndHistory(vehicle_id, user->user_id);
            if (deletion == VehicleDeletionResult::not_found)
            {
                return api::errorResponse(404, "VEHICLE_NOT_FOUND", "The requested vehicle does not exist.");
            }
            if (deletion == VehicleDeletionResult::currently_parked)
            {
                return api::errorResponse(409, "VEHICLE_CURRENTLY_PARKED", "Check out this vehicle before deleting it.");
            }
            return crow::response(204);
        }
        catch (const pqxx::foreign_key_violation &)
        {
            return api::errorResponse(409, "VEHICLE_IN_USE", "A vehicle with an active parking assignment or reservation/session history cannot be deleted.");
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Vehicle deletion failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The vehicle could not be deleted.");
        } });

    CROW_ROUTE(app, "/api/parking-areas")
    ([&database](const crow::request &)
     {
        try
        {
            const pqxx::result rows = database.executeRead(
                "SELECT pa.area_id, pa.name, pa.location, "
                "pa.operating_hours, pa.capacity, pa.status, "
                "COUNT(ps.slot_id) FILTER (WHERE ps.status IN ('available', 'occupied', 'reserved') "
                "AND NOT EXISTS (SELECT 1 FROM parking_assignments a "
                "WHERE a.slot_id = ps.slot_id AND a.checkout_at IS NULL) "
                "AND NOT EXISTS (SELECT 1 FROM parking_sessions s "
                "WHERE s.slot_id = ps.slot_id AND s.exit_time IS NULL "
                "AND s.status IN ('active', 'overstayed')) "
                "AND NOT EXISTS (SELECT 1 FROM reservations r "
                "WHERE r.slot_id = ps.slot_id AND r.status IN ('pending', 'confirmed', 'active') "
                "AND r.start_time <= CURRENT_TIMESTAMP AND r.end_time > CURRENT_TIMESTAMP)) "
                "AS available_slots "
                "FROM parking_areas pa "
                "LEFT JOIN parking_slots ps ON ps.area_id = pa.area_id "
                "GROUP BY pa.area_id "
                "ORDER BY pa.area_id");

            crow::json::wvalue response;
            response["areas"] = crow::json::wvalue::list();
            std::size_t area_index = 0;
            for (const auto &row : rows)
            {
                response["areas"][area_index++] = api::areaJson(
                    row["area_id"].as<std::int64_t>(),
                    row["name"].c_str(),
                    row["location"].c_str(),
                    row["operating_hours"].is_null() ? "" : row["operating_hours"].c_str(),
                    row["capacity"].as<int>(),
                    row["status"].c_str(),
                    row["available_slots"].as<std::int64_t>());
            }
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Parking-area query failed: " << exception.what() << std::endl;
            return api::errorResponse(
                503,
                "DATABASE_UNAVAILABLE",
                "Parking areas could not be loaded. Check the backend logs.");
        } });

    CROW_ROUTE(app, "/api/parking-areas/<uint>/slots")
    ([&database](unsigned int area_id)
     {
        try
        {
            const pqxx::result area_rows = database.executeRead(
                "SELECT area_id, name FROM parking_areas WHERE area_id = $1",
                pqxx::params{area_id});
            if (area_rows.empty())
            {
                return api::errorResponse(
                    404,
                    "PARKING_AREA_NOT_FOUND",
                    "The requested parking area does not exist.");
            }

            const pqxx::result slot_rows = database.executeRead(
                "SELECT ps.slot_id, ps.area_id, ps.slot_number, ps.vehicle_type, "
                "ps.slot_type, ps.is_ev, "
                "CASE "
                "WHEN parking.vehicle_registration IS NOT NULL THEN 'occupied' "
                "WHEN EXISTS (SELECT 1 FROM parking_sessions s "
                "WHERE s.slot_id = ps.slot_id AND s.exit_time IS NULL "
                "AND s.status IN ('active', 'overstayed')) THEN 'occupied' "
                "WHEN EXISTS (SELECT 1 FROM reservations r "
                "WHERE r.slot_id = ps.slot_id AND r.status IN ('pending', 'confirmed', 'active') "
                "AND r.start_time <= CURRENT_TIMESTAMP AND r.end_time > CURRENT_TIMESTAMP) THEN 'reserved' "
                "WHEN ps.status IN ('occupied', 'reserved') THEN 'available' "
                "ELSE ps.status END AS status, "
                "parking.vehicle_registration, parking.vehicle_type AS parked_vehicle_type, "
                "parking.is_electric AS parked_vehicle_is_electric, "
                "to_char(parking.parked_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS parked_at, "
                "EXTRACT(EPOCH FROM (CURRENT_TIMESTAMP - parking.parked_at))::BIGINT AS parked_seconds, "
                "parking.assignment_id "
                "FROM parking_slots ps "
                "LEFT JOIN LATERAL ("
                "SELECT active.vehicle_registration, active.vehicle_type, active.is_electric, "
                "active.parked_at, active.assignment_id "
                "FROM ("
                "SELECT v.registration_number AS vehicle_registration, v.vehicle_type, v.is_electric, "
                "a.parked_at, a.assignment_id "
                "FROM parking_assignments a JOIN vehicles v ON v.vehicle_id = a.vehicle_id "
                "WHERE a.slot_id = ps.slot_id AND a.checkout_at IS NULL "
                "UNION ALL "
                "SELECT v.registration_number AS vehicle_registration, v.vehicle_type, v.is_electric, "
                "s.entry_time AS parked_at, NULL::BIGINT AS assignment_id "
                "FROM parking_sessions s JOIN vehicles v ON v.vehicle_id = s.vehicle_id "
                "WHERE s.slot_id = ps.slot_id AND s.exit_time IS NULL "
                "AND s.status IN ('active', 'overstayed')"
                ") active ORDER BY active.parked_at DESC LIMIT 1"
                ") parking ON TRUE "
                "WHERE ps.area_id = $1 ORDER BY ps.slot_number",
                pqxx::params{area_id});

            crow::json::wvalue response;
            response["area"]["area_id"] = area_rows[0]["area_id"].as<std::int64_t>();
            response["area"]["name"] = area_rows[0]["name"].c_str();
            response["slots"] = crow::json::wvalue::list();
            std::size_t slot_index = 0;
            for (const auto &row : slot_rows)
            {
                crow::json::wvalue slot = api::slotJson(
                    row["slot_id"].as<std::int64_t>(),
                    row["area_id"].as<std::int64_t>(),
                    row["slot_number"].c_str(),
                    row["vehicle_type"].c_str(),
                    row["slot_type"].c_str(),
                    row["is_ev"].as<bool>(),
                    row["status"].c_str());
                if (!row["vehicle_registration"].is_null())
                {
                    slot["vehicle_registration"] = row["vehicle_registration"].c_str();
                    slot["parked_vehicle_type"] = row["parked_vehicle_type"].c_str();
                    slot["parked_vehicle_is_electric"] = row["parked_vehicle_is_electric"].as<bool>();
                    slot["parked_at"] = row["parked_at"].c_str();
                    slot["parked_seconds"] = row["parked_seconds"].as<std::int64_t>();
                    if (!row["assignment_id"].is_null())
                    {
                        slot["assignment_id"] = row["assignment_id"].as<std::int64_t>();
                    }
                }
                response["slots"][slot_index++] = std::move(slot);
            }
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Parking-slot query failed: " << exception.what() << std::endl;
            return api::errorResponse(
                503,
                "DATABASE_UNAVAILABLE",
                "Parking slots could not be loaded. Check the backend logs.");
        } });

    std::cout << "Server running on http://localhost:" << config.server_port << std::endl;

    app.port(config.server_port).multithreaded().run();

    return 0;
}
