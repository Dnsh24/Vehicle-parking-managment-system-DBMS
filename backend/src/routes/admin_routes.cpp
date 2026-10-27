#include "admin_routes.h"
#include "route_helpers.h"
#include "api/errors.h"
#include "api/admin_responses.h"
#include "validation/validation.h"

#include <iostream>
#include <array>
#include <limits>
#include <optional>
#include <utility>

namespace routes
{

void registerAdminRoutes(crow::App<crow::CORSHandler> &app, Database &database)
{
    // ---- Users: list ----
    CROW_ROUTE(app, "/api/admin/users")
    ([&database](const crow::request &request)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             const pqxx::result rows = database.executeRead(
                 "SELECT user_id, username, name, email, COALESCE(phone, '') AS phone, "
                 "role, is_active, "
                 "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\\\"T\\\"HH24:MI:SS\\\"Z\\\"') AS created_at "
                 "FROM users ORDER BY user_id");
             crow::json::wvalue response;
             response["users"] = crow::json::wvalue::list();
             std::size_t index = 0;
             for (const auto &row : rows)
             {
                 response["users"][index++] = api::adminUserJson(
                     row["user_id"].as<std::int64_t>(),
                     row["username"].c_str(),
                     row["name"].c_str(),
                     row["email"].c_str(),
                     row["phone"].c_str(),
                     row["role"].c_str(),
                     row["is_active"].as<bool>(),
                     row["created_at"].c_str());
             }
             return crow::response(response);
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin user list failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "Users could not be loaded.");
         } });

    // ---- Users: role ----
    CROW_ROUTE(app, "/api/admin/users/<uint>/role").methods(crow::HTTPMethod::PUT)
    ([&database](const crow::request &request, unsigned int target_user_id)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             if (static_cast<std::int64_t>(target_user_id) == user->user_id)
             {
                 return api::errorResponse(
                     400,
                     "CANNOT_MODIFY_OWN_ADMIN_ACCESS",
                     "An administrator cannot change their own role.");
             }
             const auto body = crow::json::load(request.body);
             const std::string role = jsonString(body, "role");
             if (!validation::allowedValue(role, "role", {"user", "admin"}).valid)
             {
                 return api::errorResponse(
                     400,
                     "INVALID_ROLE",
                     "Role must be either 'user' or 'admin'.");
             }
             const pqxx::result rows = database.executeWrite(
                 "UPDATE users SET role = $1 "
                 "WHERE user_id = $2 "
                 "RETURNING user_id, username, name, email, COALESCE(phone, '') AS phone, "
                 "role, is_active, "
                 "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\\\"T\\\"HH24:MI:SS\\\"Z\\\"') AS created_at",
                 pqxx::params{role, static_cast<std::int64_t>(target_user_id)});
             if (rows.empty())
             {
                 return api::errorResponse(404, "USER_NOT_FOUND", "The requested user does not exist.");
             }
             crow::json::wvalue response;
             response["user"] = api::adminUserJson(
                 rows[0]["user_id"].as<std::int64_t>(),
                 rows[0]["username"].c_str(),
                 rows[0]["name"].c_str(),
                 rows[0]["email"].c_str(),
                 rows[0]["phone"].c_str(),
                 rows[0]["role"].c_str(),
                 rows[0]["is_active"].as<bool>(),
                 rows[0]["created_at"].c_str());
             return crow::response(response);
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin user role update failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The user role could not be updated.");
         } });

    // ---- Users: active status ----
    CROW_ROUTE(app, "/api/admin/users/<uint>/status").methods(crow::HTTPMethod::PUT)
    ([&database](const crow::request &request, unsigned int target_user_id)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             if (static_cast<std::int64_t>(target_user_id) == user->user_id)
             {
                 return api::errorResponse(
                     400,
                     "CANNOT_MODIFY_OWN_ADMIN_ACCESS",
                     "An administrator cannot deactivate their own account.");
             }
             const auto body = crow::json::load(request.body);
             if (!body.has("is_active"))
             {
                 return api::errorResponse(
                     400,
                     "INVALID_STATUS",
                     "The 'is_active' field is required and must be a boolean.");
             }
             const std::optional<bool> is_active = jsonBoolean(body, "is_active");
             if (!is_active.has_value())
             {
                 return api::errorResponse(
                     400,
                     "INVALID_STATUS",
                     "The 'is_active' field must be a boolean.");
             }
             const pqxx::result rows = database.executeWrite(
                 "UPDATE users SET is_active = $1 "
                 "WHERE user_id = $2 "
                 "RETURNING user_id, username, name, email, COALESCE(phone, '') AS phone, "
                 "role, is_active, "
                 "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\\\"T\\\"HH24:MI:SS\\\"Z\\\"') AS created_at",
                 pqxx::params{*is_active, static_cast<std::int64_t>(target_user_id)});
             if (rows.empty())
             {
                 return api::errorResponse(404, "USER_NOT_FOUND", "The requested user does not exist.");
             }
             crow::json::wvalue response;
             response["user"] = api::adminUserJson(
                 rows[0]["user_id"].as<std::int64_t>(),
                 rows[0]["username"].c_str(),
                 rows[0]["name"].c_str(),
                 rows[0]["email"].c_str(),
                 rows[0]["phone"].c_str(),
                 rows[0]["role"].c_str(),
                 rows[0]["is_active"].as<bool>(),
                 rows[0]["created_at"].c_str());
             return crow::response(response);
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin user status update failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The user status could not be updated.");
         } });

    // ---- Users: permanent delete ----
    CROW_ROUTE(app, "/api/admin/users/<uint>").methods(crow::HTTPMethod::DELETE)
    ([&database](const crow::request &request, unsigned int target_user_id)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             if (static_cast<std::int64_t>(target_user_id) == user->user_id)
             {
                 return api::errorResponse(
                     400,
                     "CANNOT_MODIFY_OWN_ADMIN_ACCESS",
                     "An administrator cannot delete their own account.");
             }

             const UserDeletionResult result = database.deleteUserAndHistory(
                 static_cast<std::int64_t>(target_user_id));

             if (result == UserDeletionResult::not_found)
             {
                 return api::errorResponse(404, "USER_NOT_FOUND", "The requested user does not exist.");
             }
             if (result == UserDeletionResult::active_parking)
             {
                 return api::errorResponse(
                     409,
                     "CANNOT_DELETE_ACTIVE_PARKING_USER",
                     "Cannot delete user with active parking sessions or direct parking assignments.");
             }
             if (result == UserDeletionResult::future_reservations)
             {
                 return api::errorResponse(
                     409,
                     "CANNOT_DELETE_RESERVED_USER",
                     "Cannot delete user with future or active reservations.");
             }

             crow::json::wvalue response;
             response["message"] = "User account and history permanently deleted.";
             return crow::response(200, response);
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin user delete failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The user could not be deleted.");
         } });


    // ---- Areas: list ----
    CROW_ROUTE(app, "/api/admin/parking-areas")
    ([&database](const crow::request &request)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             const pqxx::result rows = database.executeRead(
                 "SELECT pa.area_id, pa.name, pa.location, pa.operating_hours, pa.capacity, "
                 "pa.status, "
                 "to_char(pa.created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\\\"T\\\"HH24:MI:SS\\\"Z\\\"') AS created_at, "
                 "COUNT(ps.slot_id) AS slot_count "
                 "FROM parking_areas pa "
                 "LEFT JOIN parking_slots ps ON ps.area_id = pa.area_id "
                 "GROUP BY pa.area_id "
                 "ORDER BY pa.area_id");
             crow::json::wvalue response;
             response["areas"] = crow::json::wvalue::list();
             std::size_t index = 0;
             for (const auto &row : rows)
             {
                 response["areas"][index++] = api::adminAreaJson(
                     row["area_id"].as<std::int64_t>(),
                     row["name"].c_str(),
                     row["location"].c_str(),
                     row["operating_hours"].is_null() ? "" : row["operating_hours"].c_str(),
                     row["capacity"].as<int>(),
                     row["status"].c_str(),
                     row["created_at"].c_str(),
                     row["slot_count"].as<std::int64_t>());
             }
             return crow::response(response);
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin area list failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "Parking areas could not be loaded.");
         } });

    // ---- Areas: create ----
    CROW_ROUTE(app, "/api/admin/parking-areas").methods(crow::HTTPMethod::POST)
    ([&database](const crow::request &request)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             const auto body = crow::json::load(request.body);
             const std::string name = jsonString(body, "name");
             const std::string location = jsonString(body, "location");
             const std::string operating_hours = jsonString(body, "operating_hours");
             if (!body.has("slot_counts"))
             {
                 return api::errorResponse(400, "INVALID_SLOT_COUNTS", "Provide a slot count for every vehicle type.");
             }
             const auto slot_counts_json = body["slot_counts"];
             const std::array<std::string, 5> slot_count_keys{"car", "bike_scooter", "truck", "ev", "handicapped"};
             std::array<std::int64_t, 5> slot_counts{};
             std::int64_t capacity = 0;
             for (std::size_t i = 0; i < slot_count_keys.size(); ++i)
             {
                 if (slot_counts_json.t() != crow::json::type::Object ||
                     !slot_counts_json.has(slot_count_keys[i]) ||
                     slot_counts_json[slot_count_keys[i]].t() != crow::json::type::Number ||
                     slot_counts_json[slot_count_keys[i]].nt() == crow::json::num_type::Floating_point ||
                     slot_counts_json[slot_count_keys[i]].i() < 0)
                 {
                     return api::errorResponse(400, "INVALID_SLOT_COUNTS", "Provide a non-negative integer slot count for every vehicle type.");
                 }
                 slot_counts[i] = slot_counts_json[slot_count_keys[i]].i();
                 capacity += slot_counts[i];
                 if (capacity > std::numeric_limits<int>::max())
                 {
                     return api::errorResponse(400, "INVALID_SLOT_COUNTS", "Total slots must not exceed the supported capacity.");
                 }
             }
             if (capacity == 0)
             {
                 return api::errorResponse(400, "INVALID_SLOT_COUNTS", "Configure at least one parking slot.");
             }
             const std::string status = jsonString(body, "status");
             const std::optional<double> latitude = jsonNumber(body, "latitude");
             const std::optional<double> longitude = jsonNumber(body, "longitude");
             if (!validation::requiredString(name, "name").valid || name.size() > 150)
             {
                 return api::errorResponse(400, "INVALID_NAME", "Name is required and must be at most 150 characters.");
             }
             if (!validation::allowedValue(status, "status", {"active", "inactive", "closed"}).valid)
             {
                 return api::errorResponse(400, "INVALID_STATUS", "Status must be 'active', 'inactive', or 'closed'.");
             }
             if (latitude.has_value() && (*latitude < -90.0 || *latitude > 90.0))
             {
                 return api::errorResponse(400, "INVALID_LATITUDE", "Latitude must be between -90 and 90.");
             }
             if (longitude.has_value() && (*longitude < -180.0 || *longitude > 180.0))
             {
                 return api::errorResponse(400, "INVALID_LONGITUDE", "Longitude must be between -180 and 180.");
             }
             const pqxx::result rows = database.executeWrite(
                 "WITH created_area AS ("
                 "INSERT INTO parking_areas (name, location, operating_hours, capacity, status, latitude, longitude) "
                 "VALUES ($1, $2, $3, $4, $5, $6, $7) "
                 "RETURNING area_id, name, location, operating_hours, capacity, status, created_at"
                 "), created_slots AS ("
                 "INSERT INTO parking_slots (area_id, slot_number, vehicle_type, slot_type, is_ev, status) "
                 "SELECT created_area.area_id, v.symbol || '-' || LPAD(n::TEXT, 2, '0'), "
                 "v.vehicle_type, v.slot_type, v.is_ev, 'available' FROM created_area "
                 "CROSS JOIN (VALUES ('car', $8::INTEGER, 'standard', FALSE, 'C'), "
                 "('bike', $9::INTEGER, 'two-wheeler', FALSE, 'B'), "
                 "('truck', $10::INTEGER, 'truck', FALSE, 'T'), "
                 "('car', $11::INTEGER, 'ev', TRUE, 'E'), "
                 "('car', $12::INTEGER, 'handicapped', FALSE, 'H')) "
                 "AS v(vehicle_type, slot_count, slot_type, is_ev, symbol) "
                 "CROSS JOIN LATERAL generate_series(1, v.slot_count) AS n "
                 "RETURNING slot_id"
                 ") SELECT created_area.area_id, created_area.name, created_area.location, "
                 "created_area.operating_hours, created_area.capacity, created_area.status, "
                 "to_char(created_area.created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\\\"T\\\"HH24:MI:SS\\\"Z\\\"') AS created_at, "
                 "(SELECT COUNT(*) FROM created_slots) AS slot_count FROM created_area",
                 pqxx::params{
                     name,
                     location,
                     operating_hours,
                     capacity,
                     status,
                     latitude,
                     longitude,
                     slot_counts[0], slot_counts[1], slot_counts[2], slot_counts[3], slot_counts[4]});
             const std::int64_t created_area_id = rows[0]["area_id"].as<std::int64_t>();
             crow::json::wvalue response;
             response["area"] = api::adminAreaJson(
                 created_area_id,
                 rows[0]["name"].c_str(),
                 rows[0]["location"].c_str(),
                 rows[0]["operating_hours"].is_null() ? "" : rows[0]["operating_hours"].c_str(),
                 rows[0]["capacity"].as<int>(),
                 rows[0]["status"].c_str(),
                 rows[0]["created_at"].c_str(),
                 rows[0]["slot_count"].as<std::int64_t>());
             return crow::response(201, response);
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin area creation failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The parking area could not be created.");
         } });

    // ---- Areas: update ----
    CROW_ROUTE(app, "/api/admin/parking-areas/<uint>").methods(crow::HTTPMethod::PUT)
    ([&database](const crow::request &request, unsigned int area_id)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             const pqxx::result existing = database.executeRead(
                 "SELECT name, location, operating_hours, capacity, status, latitude, longitude "
                 "FROM parking_areas WHERE area_id = $1",
                 pqxx::params{static_cast<std::int64_t>(area_id)});
             if (existing.empty())
             {
                 return api::errorResponse(404, "PARKING_AREA_NOT_FOUND", "The requested parking area does not exist.");
             }
             const auto body = crow::json::load(request.body);
             const std::string name = body.has("name") ? jsonString(body, "name") : existing[0]["name"].c_str();
             const std::string location = body.has("location") ? jsonString(body, "location") : existing[0]["location"].c_str();
             const std::string operating_hours = body.has("operating_hours") ? jsonString(body, "operating_hours") : (existing[0]["operating_hours"].is_null() ? "" : existing[0]["operating_hours"].c_str());
             const std::optional<std::int64_t> capacity_parsed = body.has("capacity") ? jsonInteger(body, "capacity") : std::nullopt;
             std::int64_t capacity = existing[0]["capacity"].as<std::int64_t>();
             if (capacity_parsed.has_value())
             {
                 capacity = *capacity_parsed;
             }
             else if (body.has("capacity"))
             {
                 return api::errorResponse(400, "INVALID_CAPACITY", "Capacity must be a non-negative integer.");
             }
             const std::string status = body.has("status") ? jsonString(body, "status") : existing[0]["status"].c_str();
             const std::optional<double> latitude = body.has("latitude") ? jsonNumber(body, "latitude") : (existing[0]["latitude"].is_null() ? std::optional<double>{} : std::optional<double>(existing[0]["latitude"].as<double>()));
             const std::optional<double> longitude = body.has("longitude") ? jsonNumber(body, "longitude") : (existing[0]["longitude"].is_null() ? std::optional<double>{} : std::optional<double>(existing[0]["longitude"].as<double>()));

             if (!validation::requiredString(name, "name").valid || name.size() > 150)
             {
                 return api::errorResponse(400, "INVALID_NAME", "Name is required and must be at most 150 characters.");
             }
             if (capacity < 0)
             {
                 return api::errorResponse(400, "INVALID_CAPACITY", "Capacity must be a non-negative integer.");
             }
             if (!validation::allowedValue(status, "status", {"active", "inactive", "closed"}).valid)
             {
                 return api::errorResponse(400, "INVALID_STATUS", "Status must be 'active', 'inactive', or 'closed'.");
             }
             if (latitude.has_value() && (*latitude < -90.0 || *latitude > 90.0))
             {
                 return api::errorResponse(400, "INVALID_LATITUDE", "Latitude must be between -90 and 90.");
             }
             if (longitude.has_value() && (*longitude < -180.0 || *longitude > 180.0))
             {
                 return api::errorResponse(400, "INVALID_LONGITUDE", "Longitude must be between -180 and 180.");
             }

             const std::string latitude_sql = latitude.has_value() ? std::to_string(*latitude) : "NULL";
             const std::string longitude_sql = longitude.has_value() ? std::to_string(*longitude) : "NULL";

             const pqxx::result rows = database.executeWrite(
                 "UPDATE parking_areas SET "
                 "name = $1, location = $2, operating_hours = NULLIF($3, ''), capacity = $4, status = $5, "
                 "latitude = " + latitude_sql + ", longitude = " + longitude_sql + " "
                 "WHERE area_id = $6 "
                 "RETURNING area_id, name, location, operating_hours, capacity, status, "
                 "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\\\"T\\\"HH24:MI:SS\\\"Z\\\"') AS created_at",
                 pqxx::params{
                     name,
                     location,
                     operating_hours,
                     capacity,
                     status,
                     static_cast<std::int64_t>(area_id)});
             const pqxx::result slot_count_rows = database.executeRead(
                 "SELECT COUNT(slot_id) AS slot_count FROM parking_slots WHERE area_id = $1",
                 pqxx::params{static_cast<std::int64_t>(area_id)});
             const std::int64_t current_slot_count = slot_count_rows.empty() ? 0 : slot_count_rows[0]["slot_count"].as<std::int64_t>();
             crow::json::wvalue response;
             response["area"] = api::adminAreaJson(
                 rows[0]["area_id"].as<std::int64_t>(),
                 rows[0]["name"].c_str(),
                 rows[0]["location"].c_str(),
                 rows[0]["operating_hours"].is_null() ? "" : rows[0]["operating_hours"].c_str(),
                 rows[0]["capacity"].as<int>(),
                 rows[0]["status"].c_str(),
                 rows[0]["created_at"].c_str(),
                 current_slot_count);
             return crow::response(response);
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin area update failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The parking area could not be updated.");
         } });

    // ---- Areas: delete ----
    CROW_ROUTE(app, "/api/admin/parking-areas/<uint>").methods(crow::HTTPMethod::DELETE)
    ([&database](const crow::request &request, unsigned int area_id)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             const pqxx::result exists = database.executeRead(
                 "SELECT 1 FROM parking_areas WHERE area_id = $1",
                 pqxx::params{static_cast<std::int64_t>(area_id)});
             if (exists.empty())
             {
                 return api::errorResponse(404, "PARKING_AREA_NOT_FOUND", "The requested parking area does not exist.");
             }
             database.executeWrite(
                 "DELETE FROM parking_areas WHERE area_id = $1",
                 pqxx::params{static_cast<std::int64_t>(area_id)});
             return crow::response(204);
         }
         catch (const pqxx::foreign_key_violation &)
         {
             return api::errorResponse(409, "AREA_IN_USE", "The parking area cannot be deleted because it is referenced by slots or rates.");
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin area deletion failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The parking area could not be deleted.");
         } });

    // ---- Slots: list for an area ----
    CROW_ROUTE(app, "/api/admin/parking-areas/<uint>/slots")
    ([&database](const crow::request &request, unsigned int area_id)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             const pqxx::result area_rows = database.executeRead(
                 "SELECT area_id, name FROM parking_areas WHERE area_id = $1",
                 pqxx::params{static_cast<std::int64_t>(area_id)});
             if (area_rows.empty())
             {
                 return api::errorResponse(404, "PARKING_AREA_NOT_FOUND", "The requested parking area does not exist.");
             }
             const std::string area_name = area_rows[0]["name"].c_str();
             const pqxx::result rows = database.executeRead(
                 "SELECT ps.slot_id, ps.area_id, ps.slot_number, ps.vehicle_type, "
                 "ps.slot_type, ps.is_ev, ps.status, "
                 "to_char(ps.created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\\\"T\\\"HH24:MI:SS\\\"Z\\\"') AS created_at "
                 "FROM parking_slots ps "
                 "WHERE ps.area_id = $1 "
                 "ORDER BY ps.slot_number",
                 pqxx::params{static_cast<std::int64_t>(area_id)});
             crow::json::wvalue response;
             response["area_id"] = static_cast<std::int64_t>(area_id);
             response["area_name"] = area_name;
             response["slots"] = crow::json::wvalue::list();
             std::size_t index = 0;
             for (const auto &row : rows)
             {
                 response["slots"][index++] = api::adminSlotJson(
                     row["slot_id"].as<std::int64_t>(),
                     row["area_id"].as<std::int64_t>(),
                     area_name,
                     row["slot_number"].c_str(),
                     row["vehicle_type"].c_str(),
                     row["slot_type"].c_str(),
                     row["is_ev"].as<bool>(),
                     row["status"].c_str(),
                     row["created_at"].c_str());
             }
             return crow::response(response);
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin slot list failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "Parking slots could not be loaded.");
         } });

    // ---- Slots: create ----
    CROW_ROUTE(app, "/api/admin/parking-areas/<uint>/slots").methods(crow::HTTPMethod::POST)
    ([&database](const crow::request &request, unsigned int area_id)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             const pqxx::result area_rows = database.executeRead(
                 "SELECT area_id, name FROM parking_areas WHERE area_id = $1",
                 pqxx::params{static_cast<std::int64_t>(area_id)});
             if (area_rows.empty())
             {
                 return api::errorResponse(404, "PARKING_AREA_NOT_FOUND", "The requested parking area does not exist.");
             }
             const auto body = crow::json::load(request.body);
             std::string slot_number;
             const std::string vehicle_type = jsonString(body, "vehicle_type");
             const std::string slot_type = jsonString(body, "slot_type");
             const std::optional<bool> is_ev = jsonBoolean(body, "is_ev");
             const std::string status = jsonString(body, "status");
             if (!validation::allowedValue(vehicle_type, "vehicle_type", {"car", "bike", "scooter", "truck", "bus", "other"}).valid)
             {
                 return api::errorResponse(400, "INVALID_VEHICLE_TYPE", "Unsupported vehicle type.");
             }
             if (!validation::requiredString(slot_type, "slot_type").valid || slot_type.size() > 30)
             {
                 return api::errorResponse(400, "INVALID_SLOT_TYPE", "Slot type is required.");
             }
             if (!is_ev.has_value())
             {
                 return api::errorResponse(400, "INVALID_IS_EV", "The 'is_electric' field is required and must be a boolean.");
             }
             if (!validation::allowedValue(status, "status", {"available", "reserved", "occupied", "maintenance", "unavailable"}).valid)
             {
                 return api::errorResponse(400, "INVALID_STATUS", "Unsupported slot status.");
             }
             const pqxx::result number_rows = database.executeRead(
                 "SELECT category.symbol, COALESCE(MAX(split_part(ps.slot_number, '-', 2)::INTEGER), 0) + 1 AS next_number "
                 "FROM (SELECT CASE "
                 "WHEN $1::BOOLEAN OR $2 ILIKE '%ev%' OR $2 ILIKE '%electric%' OR $2 ILIKE '%charg%' THEN 'E' "
                 "WHEN $2 ILIKE '%handicap%' OR $2 ILIKE '%accessible%' THEN 'H' "
                 "WHEN $3 IN ('bike', 'scooter') THEN 'B' "
                 "WHEN $3 = 'truck' THEN 'T' ELSE 'C' END AS symbol) AS category "
                 "LEFT JOIN parking_slots ps ON ps.area_id = $4 "
                 "AND ps.slot_number ~ ('^' || category.symbol || '-[0-9]+$') "
                 "GROUP BY category.symbol",
                 pqxx::params{*is_ev, slot_type, vehicle_type, static_cast<std::int64_t>(area_id)});
             slot_number = number_rows[0]["symbol"].c_str();
             slot_number += "-" + std::to_string(number_rows[0]["next_number"].as<std::int64_t>());
             if (slot_number.size() < 5)
             {
                 const std::size_t dash = slot_number.find('-');
                 slot_number.insert(dash + 1, 2 - (slot_number.size() - dash - 1), '0');
             }
             try
             {
                 const pqxx::result rows = database.executeWrite(
                     "INSERT INTO parking_slots (area_id, slot_number, vehicle_type, slot_type, is_ev, status) "
                     "VALUES ($1, $2, $3, $4, $5, $6) "
                     "RETURNING slot_id, area_id, slot_number, vehicle_type, slot_type, is_ev, status, "
                     "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\\\"T\\\"HH24:MI:SS\\\"Z\\\"') AS created_at",
                     pqxx::params{
                         static_cast<std::int64_t>(area_id),
                         slot_number,
                         vehicle_type,
                         slot_type,
                         *is_ev,
                         status});
                 crow::json::wvalue response;
                 response["slot"] = api::adminSlotJson(
                     rows[0]["slot_id"].as<std::int64_t>(),
                     rows[0]["area_id"].as<std::int64_t>(),
                     area_rows[0]["name"].c_str(),
                     rows[0]["slot_number"].c_str(),
                     rows[0]["vehicle_type"].c_str(),
                     rows[0]["slot_type"].c_str(),
                     rows[0]["is_ev"].as<bool>(),
                     rows[0]["status"].c_str(),
                     rows[0]["created_at"].c_str());
                 return crow::response(201, response);
             }
             catch (const pqxx::unique_violation &)
             {
                 return api::errorResponse(409, "DUPLICATE_SLOT_NUMBER", "A slot with this number already exists in the area.");
             }
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin slot creation failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The parking slot could not be created.");
         } });

    // ---- Slots: update ----
    CROW_ROUTE(app, "/api/admin/parking-slots/<uint>").methods(crow::HTTPMethod::PUT)
    ([&database](const crow::request &request, unsigned int slot_id)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             const pqxx::result existing = database.executeRead(
                 "SELECT ps.slot_id, ps.area_id, ps.slot_number, ps.vehicle_type, "
                 "ps.slot_type, ps.is_ev, ps.status, pa.name AS area_name "
                 "FROM parking_slots ps JOIN parking_areas pa ON pa.area_id = ps.area_id "
                 "WHERE ps.slot_id = $1",
                 pqxx::params{static_cast<std::int64_t>(slot_id)});
             if (existing.empty())
             {
                 return api::errorResponse(404, "PARKING_SLOT_NOT_FOUND", "The requested parking slot does not exist.");
             }
             const auto body = crow::json::load(request.body);
             std::string slot_number;
             const std::string vehicle_type = body.has("vehicle_type") ? jsonString(body, "vehicle_type") : existing[0]["vehicle_type"].c_str();
             const std::string slot_type = body.has("slot_type") ? jsonString(body, "slot_type") : existing[0]["slot_type"].c_str();
             const std::optional<bool> is_ev_parsed = body.has("is_ev") ? jsonBoolean(body, "is_ev") : std::nullopt;
             bool is_ev = existing[0]["is_ev"].as<bool>();
             if (is_ev_parsed.has_value())
             {
                 is_ev = *is_ev_parsed;
             }
             else if (body.has("is_ev"))
             {
                 return api::errorResponse(400, "INVALID_IS_EV", "The 'is_electric' field must be a boolean.");
             }
             const std::string status = body.has("status") ? jsonString(body, "status") : existing[0]["status"].c_str();

             if (!validation::allowedValue(vehicle_type, "vehicle_type", {"car", "bike", "scooter", "truck", "bus", "other"}).valid)
             {
                 return api::errorResponse(400, "INVALID_VEHICLE_TYPE", "Unsupported vehicle type.");
             }
             if (!validation::requiredString(slot_type, "slot_type").valid || slot_type.size() > 30)
             {
                 return api::errorResponse(400, "INVALID_SLOT_TYPE", "Slot type is required.");
             }
             if (!validation::allowedValue(status, "status", {"available", "reserved", "occupied", "maintenance", "unavailable"}).valid)
             {
                 return api::errorResponse(400, "INVALID_STATUS", "Unsupported slot status.");
             }
             const pqxx::result number_rows = database.executeRead(
                 "SELECT category.symbol, COALESCE(MAX(split_part(ps.slot_number, '-', 2)::INTEGER), 0) + 1 AS next_number "
                 "FROM (SELECT CASE "
                 "WHEN $1::BOOLEAN OR $2 ILIKE '%ev%' OR $2 ILIKE '%electric%' OR $2 ILIKE '%charg%' THEN 'E' "
                 "WHEN $2 ILIKE '%handicap%' OR $2 ILIKE '%accessible%' THEN 'H' "
                 "WHEN $3 IN ('bike', 'scooter') THEN 'B' "
                 "WHEN $3 = 'truck' THEN 'T' ELSE 'C' END AS symbol) AS category "
                 "LEFT JOIN parking_slots ps ON ps.area_id = $4 AND ps.slot_id <> $5 "
                 "AND ps.slot_number ~ ('^' || category.symbol || '-[0-9]+$') "
                 "GROUP BY category.symbol",
                 pqxx::params{is_ev, slot_type, vehicle_type,
                              existing[0]["area_id"].as<std::int64_t>(),
                              static_cast<std::int64_t>(slot_id)});
             const std::string symbol = number_rows[0]["symbol"].c_str();
             const std::string current_slot_number = existing[0]["slot_number"].c_str();
             if (current_slot_number.rfind(symbol + "-", 0) == 0 &&
                 current_slot_number.find_first_not_of("0123456789", symbol.size() + 1) == std::string::npos)
             {
                 slot_number = current_slot_number;
             }
             else
             {
                 const std::string digits = std::to_string(number_rows[0]["next_number"].as<std::int64_t>());
                 slot_number = symbol + "-" + (digits.size() < 2 ? "0" : "") + digits;
             }

             try
             {
                 const pqxx::result rows = database.executeWrite(
                     "UPDATE parking_slots SET "
                     "slot_number = $1, vehicle_type = $2, slot_type = $3, is_ev = $4, status = $5 "
                     "WHERE slot_id = $6 "
                     "RETURNING slot_id, area_id, slot_number, vehicle_type, slot_type, is_ev, status, "
                     "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\\\"T\\\"HH24:MI:SS\\\"Z\\\"') AS created_at",
                     pqxx::params{
                         slot_number,
                         vehicle_type,
                         slot_type,
                         is_ev,
                         status,
                         static_cast<std::int64_t>(slot_id)});
                 crow::json::wvalue response;
                 response["slot"] = api::adminSlotJson(
                     rows[0]["slot_id"].as<std::int64_t>(),
                     rows[0]["area_id"].as<std::int64_t>(),
                     existing[0]["area_name"].c_str(),
                     rows[0]["slot_number"].c_str(),
                     rows[0]["vehicle_type"].c_str(),
                     rows[0]["slot_type"].c_str(),
                     rows[0]["is_ev"].as<bool>(),
                     rows[0]["status"].c_str(),
                     rows[0]["created_at"].c_str());
                 return crow::response(response);
             }
             catch (const pqxx::unique_violation &)
             {
                 return api::errorResponse(409, "DUPLICATE_SLOT_NUMBER", "A slot with this number already exists in the area.");
             }
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin slot update failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The parking slot could not be updated.");
         } });

    // ---- Slots: delete ----
    CROW_ROUTE(app, "/api/admin/parking-slots/<uint>").methods(crow::HTTPMethod::DELETE)
    ([&database](const crow::request &request, unsigned int slot_id)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             const pqxx::result in_use = database.executeRead(
                 "SELECT EXISTS (SELECT 1 FROM parking_assignments WHERE slot_id = $1 AND checkout_at IS NULL) "
                 "OR EXISTS (SELECT 1 FROM parking_sessions WHERE slot_id = $1 AND exit_time IS NULL "
                 "AND status IN ('active', 'overstayed')) AS is_in_use",
                 pqxx::params{static_cast<std::int64_t>(slot_id)});
             if (in_use[0]["is_in_use"].as<bool>())
             {
                 return api::errorResponse(409, "SLOT_IN_USE", "The slot cannot be deleted because it is currently occupied.");
             }
             const pqxx::result exists = database.executeRead(
                 "SELECT 1 FROM parking_slots WHERE slot_id = $1",
                 pqxx::params{static_cast<std::int64_t>(slot_id)});
             if (exists.empty())
             {
                 return api::errorResponse(404, "PARKING_SLOT_NOT_FOUND", "The requested parking slot does not exist.");
             }
             database.executeWrite(
                 "DELETE FROM parking_slots WHERE slot_id = $1",
                 pqxx::params{static_cast<std::int64_t>(slot_id)});
             return crow::response(204);
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin slot deletion failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The parking slot could not be deleted.");
         } });

    // ---- Rates: list ----
    CROW_ROUTE(app, "/api/admin/parking-rates")
    ([&database](const crow::request &request)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             const pqxx::result rows = database.executeRead(
                 "SELECT pr.rate_id, pr.area_id, pa.name AS area_name, pr.vehicle_type, pr.slot_type, "
                 "pr.hourly_rate, pr.overstay_rate, pr.ev_charging_rate, "
                 "to_char(pr.created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\\\"T\\\"HH24:MI:SS\\\"Z\\\"') AS created_at "
                 "FROM parking_rates pr "
                 "JOIN parking_areas pa ON pa.area_id = pr.area_id "
                 "ORDER BY pr.rate_id");
             crow::json::wvalue response;
             response["rates"] = crow::json::wvalue::list();
             std::size_t index = 0;
             for (const auto &row : rows)
             {
                 response["rates"][index++] = api::adminRateJson(
                     row["rate_id"].as<std::int64_t>(),
                     row["area_id"].as<std::int64_t>(),
                     row["area_name"].c_str(),
                     row["vehicle_type"].c_str(),
                     row["slot_type"].c_str(),
                     row["hourly_rate"].as<double>(),
                     row["overstay_rate"].as<double>(),
                     row["ev_charging_rate"].as<double>(),
                     row["created_at"].c_str());
             }
             return crow::response(response);
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin rate list failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "Parking rates could not be loaded.");
         } });

    // ---- Rates: create ----
    CROW_ROUTE(app, "/api/admin/parking-rates").methods(crow::HTTPMethod::POST)
    ([&database](const crow::request &request)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             const auto body = crow::json::load(request.body);
             const std::optional<std::int64_t> area_id = jsonInteger(body, "area_id");
             const std::string vehicle_type = jsonString(body, "vehicle_type");
             const std::string slot_type = jsonString(body, "slot_type");
             const std::optional<double> hourly_rate = jsonNumber(body, "hourly_rate");
             const std::optional<double> overstay_rate = jsonNumber(body, "overstay_rate");
             const std::optional<double> ev_charging_rate = jsonNumber(body, "ev_charging_rate");
             if (!area_id.has_value())
             {
                 return api::errorResponse(400, "INVALID_AREA", "A valid area is required.");
             }
             const pqxx::result area_rows = database.executeRead(
                 "SELECT area_id FROM parking_areas WHERE area_id = $1",
                 pqxx::params{*area_id});
             if (area_rows.empty())
             {
                 return api::errorResponse(404, "PARKING_AREA_NOT_FOUND", "The requested parking area does not exist.");
             }
             if (!validation::allowedValue(vehicle_type, "vehicle_type", {"car", "bike", "scooter", "truck", "bus", "other"}).valid)
             {
                 return api::errorResponse(400, "INVALID_VEHICLE_TYPE", "Unsupported vehicle type.");
             }
             if (!validation::requiredString(slot_type, "slot_type").valid || slot_type.size() > 30)
             {
                 return api::errorResponse(400, "INVALID_SLOT_TYPE", "Slot type is required.");
             }
             if (!hourly_rate.has_value() || *hourly_rate < 0)
             {
                 return api::errorResponse(400, "INVALID_HOURLY_RATE", "Hourly rate must be a non-negative number.");
             }
             if (!overstay_rate.has_value() || *overstay_rate < 0)
             {
                 return api::errorResponse(400, "INVALID_OVERSTAY_RATE", "Overstay rate must be a non-negative number.");
             }
             if (!ev_charging_rate.has_value() || *ev_charging_rate < 0)
             {
                 return api::errorResponse(400, "INVALID_EV_CHARGING_RATE", "EV charging rate must be a non-negative number.");
             }
             try
             {
                 const pqxx::result rows = database.executeWrite(
                     "INSERT INTO parking_rates (area_id, vehicle_type, slot_type, hourly_rate, overstay_rate, ev_charging_rate) "
                     "VALUES ($1, $2, $3, $4, $5, $6) "
                     "RETURNING rate_id, area_id, vehicle_type, slot_type, hourly_rate, overstay_rate, ev_charging_rate, "
                     "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\\\"T\\\"HH24:MI:SS\\\"Z\\\"') AS created_at",
                     pqxx::params{
                         *area_id,
                         vehicle_type,
                         slot_type,
                         *hourly_rate,
                         *overstay_rate,
                         *ev_charging_rate});
                 crow::json::wvalue response;
                 response["rate"] = api::adminRateJson(
                     rows[0]["rate_id"].as<std::int64_t>(),
                     rows[0]["area_id"].as<std::int64_t>(),
                     area_rows[0]["name"].c_str(),
                     rows[0]["vehicle_type"].c_str(),
                     rows[0]["slot_type"].c_str(),
                     rows[0]["hourly_rate"].as<double>(),
                     rows[0]["overstay_rate"].as<double>(),
                     rows[0]["ev_charging_rate"].as<double>(),
                     rows[0]["created_at"].c_str());
                 return crow::response(201, response);
             }
             catch (const pqxx::unique_violation &)
             {
                 return api::errorResponse(409, "DUPLICATE_RATE", "A rate already exists for this area, vehicle type, and slot type.");
             }
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin rate creation failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The parking rate could not be created.");
         } });

    // ---- Rates: update ----
    CROW_ROUTE(app, "/api/admin/parking-rates/<uint>").methods(crow::HTTPMethod::PUT)
    ([&database](const crow::request &request, unsigned int rate_id)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             const pqxx::result existing = database.executeRead(
                 "SELECT pr.rate_id, pr.area_id, pa.name AS area_name, pr.vehicle_type, pr.slot_type, "
                 "pr.hourly_rate, pr.overstay_rate, pr.ev_charging_rate "
                 "FROM parking_rates pr JOIN parking_areas pa ON pa.area_id = pr.area_id "
                 "WHERE pr.rate_id = $1",
                 pqxx::params{static_cast<std::int64_t>(rate_id)});
             if (existing.empty())
             {
                 return api::errorResponse(404, "PARKING_RATE_NOT_FOUND", "The requested parking rate does not exist.");
             }
             const auto body = crow::json::load(request.body);
             const std::optional<std::int64_t> area_id_parsed = body.has("area_id") ? jsonInteger(body, "area_id") : std::nullopt;
             std::int64_t area_id = existing[0]["area_id"].as<std::int64_t>();
             if (area_id_parsed.has_value())
             {
                 area_id = *area_id_parsed;
             }
             else if (body.has("area_id"))
             {
                 return api::errorResponse(400, "INVALID_AREA", "A valid area is required.");
             }
             const std::string vehicle_type = body.has("vehicle_type") ? jsonString(body, "vehicle_type") : existing[0]["vehicle_type"].c_str();
             const std::string slot_type = body.has("slot_type") ? jsonString(body, "slot_type") : existing[0]["slot_type"].c_str();
             const std::optional<double> hourly_rate_parsed = body.has("hourly_rate") ? jsonNumber(body, "hourly_rate") : std::nullopt;
             double hourly_rate = existing[0]["hourly_rate"].as<double>();
             if (hourly_rate_parsed.has_value())
             {
                 hourly_rate = *hourly_rate_parsed;
             }
             else if (body.has("hourly_rate"))
             {
                 return api::errorResponse(400, "INVALID_HOURLY_RATE", "Hourly rate must be a non-negative number.");
             }
             const std::optional<double> overstay_rate_parsed = body.has("overstay_rate") ? jsonNumber(body, "overstay_rate") : std::nullopt;
             double overstay_rate = existing[0]["overstay_rate"].as<double>();
             if (overstay_rate_parsed.has_value())
             {
                 overstay_rate = *overstay_rate_parsed;
             }
             else if (body.has("overstay_rate"))
             {
                 return api::errorResponse(400, "INVALID_OVERSTAY_RATE", "Overstay rate must be a non-negative number.");
             }
             const std::optional<double> ev_charging_rate_parsed = body.has("ev_charging_rate") ? jsonNumber(body, "ev_charging_rate") : std::nullopt;
             double ev_charging_rate = existing[0]["ev_charging_rate"].as<double>();
             if (ev_charging_rate_parsed.has_value())
             {
                 ev_charging_rate = *ev_charging_rate_parsed;
             }
             else if (body.has("ev_charging_rate"))
             {
                 return api::errorResponse(400, "INVALID_EV_CHARGING_RATE", "EV charging rate must be a non-negative number.");
             }

             const pqxx::result area_rows = database.executeRead(
                 "SELECT area_id, name FROM parking_areas WHERE area_id = $1",
                 pqxx::params{area_id});
             if (area_rows.empty())
             {
                 return api::errorResponse(404, "PARKING_AREA_NOT_FOUND", "The requested parking area does not exist.");
             }
             if (!validation::allowedValue(vehicle_type, "vehicle_type", {"car", "bike", "scooter", "truck", "bus", "other"}).valid)
             {
                 return api::errorResponse(400, "INVALID_VEHICLE_TYPE", "Unsupported vehicle type.");
             }
             if (!validation::requiredString(slot_type, "slot_type").valid || slot_type.size() > 30)
             {
                 return api::errorResponse(400, "INVALID_SLOT_TYPE", "Slot type is required.");
             }
             if (hourly_rate < 0 || overstay_rate < 0 || ev_charging_rate < 0)
             {
                 return api::errorResponse(400, "INVALID_RATE", "Rates must be non-negative numbers.");
             }
             try
             {
                 const pqxx::result rows = database.executeWrite(
                     "UPDATE parking_rates SET "
                     "area_id = $1, vehicle_type = $2, slot_type = $3, hourly_rate = $4, overstay_rate = $5, ev_charging_rate = $6 "
                     "WHERE rate_id = $7 "
                     "RETURNING rate_id, area_id, vehicle_type, slot_type, hourly_rate, overstay_rate, ev_charging_rate, "
                     "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\\\"T\\\"HH24:MI:SS\\\"Z\\\"') AS created_at",
                     pqxx::params{
                         area_id,
                         vehicle_type,
                         slot_type,
                         hourly_rate,
                         overstay_rate,
                         ev_charging_rate,
                         static_cast<std::int64_t>(rate_id)});
                 crow::json::wvalue response;
                 response["rate"] = api::adminRateJson(
                     rows[0]["rate_id"].as<std::int64_t>(),
                     rows[0]["area_id"].as<std::int64_t>(),
                     area_rows[0]["name"].c_str(),
                     rows[0]["vehicle_type"].c_str(),
                     rows[0]["slot_type"].c_str(),
                     rows[0]["hourly_rate"].as<double>(),
                     rows[0]["overstay_rate"].as<double>(),
                     rows[0]["ev_charging_rate"].as<double>(),
                     rows[0]["created_at"].c_str());
                 return crow::response(response);
             }
             catch (const pqxx::unique_violation &)
             {
                 return api::errorResponse(409, "DUPLICATE_RATE", "A rate already exists for this area, vehicle type, and slot type.");
             }
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin rate update failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The parking rate could not be updated.");
         } });

    // ---- Rates: delete ----
    CROW_ROUTE(app, "/api/admin/parking-rates/<uint>").methods(crow::HTTPMethod::DELETE)
    ([&database](const crow::request &request, unsigned int rate_id)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             const pqxx::result exists = database.executeRead(
                 "SELECT 1 FROM parking_rates WHERE rate_id = $1",
                 pqxx::params{static_cast<std::int64_t>(rate_id)});
             if (exists.empty())
             {
                 return api::errorResponse(404, "PARKING_RATE_NOT_FOUND", "The requested parking rate does not exist.");
             }
             database.executeWrite(
                 "DELETE FROM parking_rates WHERE rate_id = $1",
                 pqxx::params{static_cast<std::int64_t>(rate_id)});
             return crow::response(204);
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin rate deletion failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The parking rate could not be deleted.");
         } });

    // ---- Maintenance: list ----
    CROW_ROUTE(app, "/api/admin/maintenance")
    ([&database](const crow::request &request)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             const pqxx::result rows = database.executeRead(
                 "SELECT m.maintenance_id, m.slot_id, ps.slot_number, ps.area_id, pa.name AS area_name, "
                 "m.reason, "
                 "to_char(m.start_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\\\"T\\\"HH24:MI:SS\\\"Z\\\"') AS start_time, "
                 "CASE WHEN m.end_time IS NULL THEN '' ELSE to_char(m.end_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\\\"T\\\"HH24:MI:SS\\\"Z\\\"') END AS end_time, "
                 "m.status, "
                 "to_char(m.created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\\\"T\\\"HH24:MI:SS\\\"Z\\\"') AS created_at "
                 "FROM maintenance m "
                 "JOIN parking_slots ps ON ps.slot_id = m.slot_id "
                 "JOIN parking_areas pa ON pa.area_id = ps.area_id "
                 "ORDER BY m.created_at DESC");
             crow::json::wvalue response;
             response["maintenance"] = crow::json::wvalue::list();
             std::size_t index = 0;
             for (const auto &row : rows)
             {
                 response["maintenance"][index++] = api::adminMaintenanceJson(
                     row["maintenance_id"].as<std::int64_t>(),
                     row["slot_id"].as<std::int64_t>(),
                     row["slot_number"].c_str(),
                     row["area_id"].as<std::int64_t>(),
                     row["area_name"].c_str(),
                     row["reason"].c_str(),
                     row["start_time"].c_str(),
                     row["end_time"].c_str(),
                     row["status"].c_str(),
                     row["created_at"].c_str());
             }
             return crow::response(response);
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin maintenance list failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "Maintenance records could not be loaded.");
         } });

    // ---- Maintenance: create ----
    CROW_ROUTE(app, "/api/admin/maintenance").methods(crow::HTTPMethod::POST)
    ([&database](const crow::request &request)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             const auto body = crow::json::load(request.body);
             const std::optional<std::int64_t> slot_id = jsonInteger(body, "slot_id");
             const std::string reason = jsonString(body, "reason");
             const std::string start_time = jsonString(body, "start_time");
             const std::string end_time = jsonString(body, "end_time");
             const std::string status = jsonString(body, "status");
             if (!slot_id.has_value())
             {
                 return api::errorResponse(400, "INVALID_SLOT", "A valid slot is required.");
             }
             const pqxx::result slot_rows = database.executeRead(
                 "SELECT ps.slot_id, ps.slot_number, ps.area_id, pa.name AS area_name "
                 "FROM parking_slots ps JOIN parking_areas pa ON pa.area_id = ps.area_id "
                 "WHERE ps.slot_id = $1",
                 pqxx::params{*slot_id});
             if (slot_rows.empty())
             {
                 return api::errorResponse(404, "PARKING_SLOT_NOT_FOUND", "The requested parking slot does not exist.");
             }
             if (!validation::requiredString(reason, "reason").valid)
             {
                 return api::errorResponse(400, "INVALID_REASON", "A maintenance reason is required.");
             }
             if (!validation::timestamp(start_time, "start_time").valid)
             {
                 return api::errorResponse(400, "INVALID_START_TIME", "Start time must be an ISO-8601 timestamp.");
             }
             if (!end_time.empty() && !validation::timestamp(end_time, "end_time").valid)
             {
                 return api::errorResponse(400, "INVALID_END_TIME", "End time must be an ISO-8601 timestamp.");
             }
             if (!validation::allowedValue(status, "status", {"scheduled", "in_progress", "completed", "cancelled"}).valid)
             {
                 return api::errorResponse(400, "INVALID_MAINTENANCE_STATUS", "Unsupported maintenance status.");
             }
             const pqxx::result rows = database.executeWrite(
                 "INSERT INTO maintenance (slot_id, reason, start_time, end_time, status) "
                 "VALUES ($1, $2, $3, NULLIF($4, '')::timestamptz, $5) "
                 "RETURNING maintenance_id, slot_id, reason, start_time, end_time, status, "
                 "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\\\"T\\\"HH24:MI:SS\\\"Z\\\"') AS created_at",
                 pqxx::params{
                     *slot_id,
                     reason,
                     start_time,
                     end_time,
                     status});
             crow::json::wvalue response;
             response["maintenance"] = api::adminMaintenanceJson(
                 rows[0]["maintenance_id"].as<std::int64_t>(),
                 rows[0]["slot_id"].as<std::int64_t>(),
                 slot_rows[0]["slot_number"].c_str(),
                 slot_rows[0]["area_id"].as<std::int64_t>(),
                 slot_rows[0]["area_name"].c_str(),
                 rows[0]["reason"].c_str(),
                 rows[0]["start_time"].c_str(),
                 rows[0]["end_time"].is_null() ? "" : rows[0]["end_time"].c_str(),
                 rows[0]["status"].c_str(),
                 rows[0]["created_at"].c_str());
             return crow::response(201, response);
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin maintenance creation failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The maintenance record could not be created.");
         } });

    // ---- Maintenance: update ----
    CROW_ROUTE(app, "/api/admin/maintenance/<uint>").methods(crow::HTTPMethod::PUT)
    ([&database](const crow::request &request, unsigned int maintenance_id)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             const pqxx::result existing = database.executeRead(
                 "SELECT m.maintenance_id, m.slot_id, m.reason, m.start_time, m.end_time, m.status "
                 "FROM maintenance m WHERE m.maintenance_id = $1",
                 pqxx::params{static_cast<std::int64_t>(maintenance_id)});
             if (existing.empty())
             {
                 return api::errorResponse(404, "MAINTENANCE_NOT_FOUND", "The requested maintenance record does not exist.");
             }
             const auto body = crow::json::load(request.body);
             const std::optional<std::int64_t> slot_id = body.has("slot_id") ? jsonInteger(body, "slot_id") : existing[0]["slot_id"].as<std::int64_t>();
             const std::string reason = body.has("reason") ? jsonString(body, "reason") : existing[0]["reason"].c_str();
             const std::string start_time = body.has("start_time") ? jsonString(body, "start_time") : existing[0]["start_time"].c_str();
             const std::string end_time = body.has("end_time") ? jsonString(body, "end_time") : (existing[0]["end_time"].is_null() ? "" : existing[0]["end_time"].c_str());
             const std::string status = body.has("status") ? jsonString(body, "status") : existing[0]["status"].c_str();

             const pqxx::result slot_rows = database.executeRead(
                 "SELECT ps.slot_id, ps.slot_number, ps.area_id, pa.name AS area_name "
                 "FROM parking_slots ps JOIN parking_areas pa ON pa.area_id = ps.area_id "
                 "WHERE ps.slot_id = $1",
                 pqxx::params{*slot_id});
             if (slot_rows.empty())
             {
                 return api::errorResponse(404, "PARKING_SLOT_NOT_FOUND", "The requested parking slot does not exist.");
             }
             if (!validation::requiredString(reason, "reason").valid)
             {
                 return api::errorResponse(400, "INVALID_REASON", "A maintenance reason is required.");
             }
             if (!validation::timestamp(start_time, "start_time").valid)
             {
                 return api::errorResponse(400, "INVALID_START_TIME", "Start time must be an ISO-8601 timestamp.");
             }
             if (!end_time.empty() && !validation::timestamp(end_time, "end_time").valid)
             {
                 return api::errorResponse(400, "INVALID_END_TIME", "End time must be an ISO-8601 timestamp.");
             }
             if (!validation::allowedValue(status, "status", {"scheduled", "in_progress", "completed", "cancelled"}).valid)
             {
                 return api::errorResponse(400, "INVALID_MAINTENANCE_STATUS", "Unsupported maintenance status.");
             }
             const pqxx::result rows = database.executeWrite(
                 "UPDATE maintenance SET "
                 "slot_id = $1, reason = $2, start_time = $3, end_time = NULLIF($4, '')::timestamptz, status = $5 "
                 "WHERE maintenance_id = $6 "
                 "RETURNING maintenance_id, slot_id, reason, start_time, end_time, status, "
                 "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\\\"T\\\"HH24:MI:SS\\\"Z\\\"') AS created_at",
                 pqxx::params{
                     *slot_id,
                     reason,
                     start_time,
                     end_time,
                     status,
                     static_cast<std::int64_t>(maintenance_id)});
             crow::json::wvalue response;
             response["maintenance"] = api::adminMaintenanceJson(
                 rows[0]["maintenance_id"].as<std::int64_t>(),
                 rows[0]["slot_id"].as<std::int64_t>(),
                 slot_rows[0]["slot_number"].c_str(),
                 slot_rows[0]["area_id"].as<std::int64_t>(),
                 slot_rows[0]["area_name"].c_str(),
                 rows[0]["reason"].c_str(),
                 rows[0]["start_time"].c_str(),
                 rows[0]["end_time"].is_null() ? "" : rows[0]["end_time"].c_str(),
                 rows[0]["status"].c_str(),
                 rows[0]["created_at"].c_str());
             return crow::response(response);
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin maintenance update failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The maintenance record could not be updated.");
         } });

    // ---- Maintenance: delete ----
    CROW_ROUTE(app, "/api/admin/maintenance/<uint>").methods(crow::HTTPMethod::DELETE)
    ([&database](const crow::request &request, unsigned int maintenance_id)
     {
         try
         {
             const auto user = findAuthenticatedUser(database, request);
             if (!user)
             {
                 return unauthorized();
             }
             if (user->role != "admin")
             {
                 return forbidden();
             }
             const pqxx::result exists = database.executeRead(
                 "SELECT 1 FROM maintenance WHERE maintenance_id = $1",
                 pqxx::params{static_cast<std::int64_t>(maintenance_id)});
             if (exists.empty())
             {
                 return api::errorResponse(404, "MAINTENANCE_NOT_FOUND", "The requested maintenance record does not exist.");
             }
             database.executeWrite(
                 "DELETE FROM maintenance WHERE maintenance_id = $1",
                 pqxx::params{static_cast<std::int64_t>(maintenance_id)});
             return crow::response(204);
         }
         catch (const std::exception &exception)
         {
             std::cerr << "Admin maintenance deletion failed: " << exception.what() << std::endl;
             return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The maintenance record could not be deleted.");
         } });
}

} // namespace routes
