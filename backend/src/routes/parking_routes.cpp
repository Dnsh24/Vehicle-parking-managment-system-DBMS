#include "parking_routes.h"
#include "route_helpers.h"
#include "api/errors.h"
#include "api/parking_responses.h"

#include <iostream>
#include <utility>

namespace routes
{

void registerParkingRoutes(
    crow::App<crow::CORSHandler> &app,
    Database &database,
    const DatabaseCheckResult &database_check,
    const std::string &database_name)
{
    CROW_ROUTE(app, "/api/hello")
    ([]()
     {
        crow::json::wvalue response;
        response["message"] = "C++ backend is working";
        return response; });

    CROW_ROUTE(app, "/api/database")
    ([database_check, database_name]()
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
                "ps.slot_type, (ps.is_ev OR ps.slot_type ~* '(^|[^[:alnum:]])ev([^[:alnum:]]|$)' "
                "OR ps.slot_type ILIKE '%electric%' OR ps.slot_type ILIKE '%charg%') AS is_ev, "
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
                "parking.planned_end_time, "
                "parking.assignment_id "
                "FROM parking_slots ps "
                "LEFT JOIN LATERAL ("
                "SELECT active.vehicle_registration, active.vehicle_type, active.is_electric, "
                "active.parked_at, active.planned_end_time, active.assignment_id "
                "FROM ("
                "SELECT v.registration_number AS vehicle_registration, v.vehicle_type, v.is_electric, "
                "a.parked_at, "
                "to_char(a.planned_end_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS planned_end_time, "
                "a.assignment_id "
                "FROM parking_assignments a JOIN vehicles v ON v.vehicle_id = a.vehicle_id "
                "WHERE a.slot_id = ps.slot_id AND a.checkout_at IS NULL "
                "UNION ALL "
                "SELECT v.registration_number AS vehicle_registration, v.vehicle_type, v.is_electric, "
                "s.entry_time AS parked_at, "
                "to_char(r.end_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS planned_end_time, "
                "NULL::BIGINT AS assignment_id "
                "FROM parking_sessions s "
                "JOIN reservations r ON r.reservation_id = s.reservation_id "
                "JOIN vehicles v ON v.vehicle_id = s.vehicle_id "
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
                    if (!row["planned_end_time"].is_null())
                    {
                        slot["planned_end_time"] = row["planned_end_time"].c_str();
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
}

} // namespace routes
