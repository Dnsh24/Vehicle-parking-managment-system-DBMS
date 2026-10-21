#include "vehicle_routes.h"
#include "route_helpers.h"
#include "api/errors.h"
#include "api/parking_responses.h"
#include "api/vehicle_responses.h"
#include "validation/validation.h"

#include <iostream>
#include <utility>

namespace routes
{

void registerVehicleRoutes(crow::App<crow::CORSHandler> &app, Database &database)
{
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
            const VehicleParkingResult result = database.parkVehicle(
                user->user_id,
                vehicle_id,
                slot_id);
            if (result.status == VehicleParkingStatus::vehicle_not_found)
            {
                return api::errorResponse(404, "VEHICLE_NOT_FOUND", "The requested vehicle does not exist.");
            }
            if (result.status == VehicleParkingStatus::slot_unavailable)
            {
                return api::errorResponse(409, "PARKING_SLOT_UNAVAILABLE", "The slot is no longer available or is incompatible with this vehicle.");
            }

            crow::json::wvalue response;
            response["parking"]["assignment_id"] = result.assignment_id;
            response["parking"]["vehicle_id"] = vehicle_id;
            response["parking"]["slot_id"] = result.slot_id;
            response["parking"]["area_id"] = result.area_id;
            response["parking"]["slot_number"] = result.slot_number;
            response["parking"]["area_name"] = result.area_name;
            response["parking"]["parked_at"] = result.parked_at;
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
}

} // namespace routes
