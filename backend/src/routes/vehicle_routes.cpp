#include "vehicle_routes.h"
#include "route_helpers.h"
#include "api/errors.h"
#include "api/parking_responses.h"
#include "api/vehicle_responses.h"
#include "api/session_responses.h"
#include "api/fee_calculator.h"
#include <algorithm>
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
                "v.needs_accessible_parking, "
                "a.assignment_id, a.parked_at, a.planned_end_time, a.slot_id, a.slot_number, a.area_name "
                "FROM vehicles v "
                "LEFT JOIN LATERAL ("
                "SELECT active.assignment_id, active.parked_at, active.planned_end_time, active.slot_id, "
                "active.slot_number, active.area_name "
                "FROM ("
                "SELECT a.assignment_id, a.parked_at, "
                "to_char(a.planned_end_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS planned_end_time, "
                "ps.slot_id, ps.slot_number, pa.name AS area_name "
                "FROM parking_assignments a "
                "JOIN parking_slots ps ON ps.slot_id = a.slot_id "
                "JOIN parking_areas pa ON pa.area_id = ps.area_id "
                "WHERE a.vehicle_id = v.vehicle_id AND a.checkout_at IS NULL "
                "UNION ALL "
                "SELECT NULL::BIGINT, s.entry_time, "
                "to_char(r.end_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS planned_end_time, "
                "ps.slot_id, ps.slot_number, pa.name "
                "FROM parking_sessions s "
                "JOIN reservations r ON r.reservation_id = s.reservation_id "
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
                    row["is_electric"].as<bool>(),
                    row["needs_accessible_parking"].as<bool>());
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
                    if (!row["planned_end_time"].is_null())
                    {
                        vehicle["parking"]["planned_end_time"] = row["planned_end_time"].c_str();
                    }
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
            database.cancelOverdueReservations();
            const pqxx::result vehicle_rows = database.executeRead(
                "SELECT vehicle_type, is_electric, needs_accessible_parking "
                "FROM vehicles WHERE vehicle_id = $1 AND user_id = $2",
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

            const pqxx::result reservation_rows = database.executeRead(
                "SELECT r.reservation_id, r.status, pa.name AS area_name, ps.slot_number, "
                "to_char(r.start_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS start_time, "
                "to_char(r.end_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS end_time "
                "FROM reservations r JOIN parking_slots ps ON ps.slot_id = r.slot_id "
                "JOIN parking_areas pa ON pa.area_id = ps.area_id "
                "WHERE r.vehicle_id = $1 AND r.status IN ('pending', 'confirmed', 'active') "
                "AND r.end_time > CURRENT_TIMESTAMP "
                "ORDER BY r.start_time, r.reservation_id LIMIT 1",
                pqxx::params{vehicle_id});
            const pqxx::result rows = database.executeRead(
                "SELECT ps.slot_id, ps.area_id, ps.slot_number, ps.vehicle_type, "
                "ps.slot_type, (ps.is_ev OR ps.slot_type ~* '(^|[^[:alnum:]])ev([^[:alnum:]]|$)' "
                "OR ps.slot_type ILIKE '%electric%' OR ps.slot_type ILIKE '%charg%') AS is_ev, "
                "'available' AS status, pa.name AS area_name "
                "FROM parking_slots ps "
                "JOIN parking_areas pa ON pa.area_id = ps.area_id "
                "WHERE (ps.status = 'available' OR ps.status IN ('occupied', 'reserved')) "
                "AND pa.status = 'active' "
                "AND (ps.vehicle_type = $1 OR ($1 = 'scooter' AND ps.vehicle_type = 'bike')) "
                "AND (NOT (ps.is_ev OR ps.slot_type ~* '(^|[^[:alnum:]])ev([^[:alnum:]]|$)' "
                "OR ps.slot_type ILIKE '%electric%' OR ps.slot_type ILIKE '%charg%') OR $3) "
                "AND ($4 OR (ps.slot_type NOT ILIKE '%handicap%' AND ps.slot_type NOT ILIKE '%accessible%')) "
                "AND NOT EXISTS (SELECT 1 FROM parking_assignments a "
                "WHERE a.slot_id = ps.slot_id AND a.checkout_at IS NULL) "
                "AND NOT EXISTS (SELECT 1 FROM parking_assignments a "
                "WHERE a.vehicle_id = $2 AND a.checkout_at IS NULL) "
                "AND NOT EXISTS (SELECT 1 FROM parking_sessions s "
                "WHERE s.vehicle_id = $2 AND s.exit_time IS NULL "
                "AND s.status IN ('active', 'overstayed')) "
                "AND NOT EXISTS (SELECT 1 FROM reservations r "
                "WHERE r.slot_id = ps.slot_id "
                "AND r.status IN ('pending', 'confirmed', 'active') "
                "AND r.end_time > CURRENT_TIMESTAMP) "
                "AND NOT EXISTS (SELECT 1 FROM parking_sessions s "
                "WHERE s.slot_id = ps.slot_id AND s.exit_time IS NULL "
                "AND s.status IN ('active', 'overstayed')) "
                "AND NOT EXISTS (SELECT 1 FROM maintenance m "
                "WHERE m.slot_id = ps.slot_id AND m.status IN ('scheduled', 'in_progress') "
                "AND (m.end_time IS NULL OR m.end_time > CURRENT_TIMESTAMP)) "
                "ORDER BY pa.area_id, ps.slot_number",
                pqxx::params{
                    vehicle_rows[0]["vehicle_type"].c_str(),
                    vehicle_id,
                    vehicle_rows[0]["is_electric"].as<bool>(),
                    vehicle_rows[0]["needs_accessible_parking"].as<bool>()});

            crow::json::wvalue response;
            response["slots"] = crow::json::wvalue::list();
            if (!reservation_rows.empty())
            {
                response["reservation"]["reservation_id"] = reservation_rows[0]["reservation_id"].as<std::int64_t>();
                response["reservation"]["status"] = reservation_rows[0]["status"].c_str();
                response["reservation"]["area_name"] = reservation_rows[0]["area_name"].c_str();
                response["reservation"]["slot_number"] = reservation_rows[0]["slot_number"].c_str();
                response["reservation"]["start_time"] = reservation_rows[0]["start_time"].c_str();
                response["reservation"]["end_time"] = reservation_rows[0]["end_time"].c_str();
            }
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
            int duration_minutes = 120;
            if (body.has("duration_minutes") &&
                body["duration_minutes"].t() == crow::json::type::Number)
            {
                const int mins = body["duration_minutes"].i();
                if (mins < 1 || mins > 1440)
                {
                    return api::errorResponse(400, "INVALID_DURATION", "duration_minutes must be between 1 and 1440 minutes.");
                }
                duration_minutes = mins;
            }
            else if (body.has("duration_hours") &&
                     body["duration_hours"].t() == crow::json::type::Number)
            {
                double hours = 0.0;
                if (body["duration_hours"].nt() == crow::json::num_type::Floating_point)
                {
                    hours = body["duration_hours"].d();
                }
                else
                {
                    hours = static_cast<double>(body["duration_hours"].i());
                }
                if (hours <= 0.0 || hours > 24.0)
                {
                    return api::errorResponse(400, "INVALID_DURATION", "duration_hours must be between 0.01 and 24 hours.");
                }
                duration_minutes = std::max(1, std::min(1440, static_cast<int>(std::round(hours * 60.0))));
            }

            const pqxx::result reservation_conflict = database.executeRead(
                "SELECT EXISTS (SELECT 1 FROM reservations r "
                "LEFT JOIN vehicles v ON v.vehicle_id = r.vehicle_id "
                "WHERE (r.slot_id = $2 OR (r.vehicle_id = $1 AND v.user_id = $3)) "
                "AND r.status IN ('pending', 'confirmed', 'active') "
                "AND r.start_time < CURRENT_TIMESTAMP + ($4::INTEGER * INTERVAL '1 minute') "
                "AND r.end_time > CURRENT_TIMESTAMP) AS has_conflict",
                pqxx::params{static_cast<std::int64_t>(vehicle_id), slot_id, user->user_id, duration_minutes});
            if (!reservation_conflict.empty() && reservation_conflict[0]["has_conflict"].as<bool>())
            {
                return api::errorResponse(
                    409,
                    "PARKING_RESERVATION_CONFLICT",
                    "This stay overlaps a reservation. Choose a shorter duration, or use Reservations to check in or cancel the booking.");
            }

            const VehicleParkingResult result = database.parkVehicle(
                user->user_id,
                vehicle_id,
                slot_id,
                duration_minutes);
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
            response["parking"]["planned_end_time"] = result.planned_end_time;
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

    CROW_ROUTE(app, "/api/vehicles/<uint>/fee-summary").methods(crow::HTTPMethod::GET)
    ([&database](const crow::request &request, unsigned int vehicle_id)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }

            // Check if vehicle has an active parking session
            const pqxx::result session_rows = database.executeRead(
                "SELECT session_id FROM parking_sessions "
                "WHERE vehicle_id = $1 AND user_id = $2 AND exit_time IS NULL AND status IN ('active', 'overstayed')",
                pqxx::params{vehicle_id, user->user_id});

            if (!session_rows.empty())
            {
                const std::int64_t session_id = session_rows[0]["session_id"].as<std::int64_t>();
                const auto summary = database.getSessionFeeSummary(user->user_id, session_id);
                if (summary.has_value())
                {
                    crow::json::wvalue response;
                    response["session"] = api::sessionFeeSummaryJson(*summary);
                    return crow::response(200, response);
                }
            }

            // Check if vehicle has an active direct parking assignment
            const pqxx::result assign_rows = database.executeRead(
                "SELECT a.assignment_id, a.slot_id, ps.slot_number, ps.slot_type, ps.area_id, "
                "pa.name AS area_name, pa.location, "
                "v.registration_number, v.vehicle_type, "
                "to_char(a.parked_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS entry_time_str, "
                "to_char(clock_timestamp() AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS exit_time_str, "
                "to_char(COALESCE(a.planned_end_time, a.parked_at + INTERVAL '2 hours') AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS planned_end_time_str, "
                "EXTRACT(EPOCH FROM a.parked_at)::BIGINT AS start_epoch, "
                "EXTRACT(EPOCH FROM COALESCE(a.planned_end_time, a.parked_at + INTERVAL '2 hours'))::BIGINT AS planned_end_epoch, "
                "EXTRACT(EPOCH FROM clock_timestamp())::BIGINT AS exit_epoch, "
                "COALESCE(pr.hourly_rate, 40.00) AS hourly_rate, "
                "COALESCE(pr.overstay_rate, 60.00) AS overstay_rate "
                "FROM parking_assignments a "
                "JOIN vehicles v ON v.vehicle_id = a.vehicle_id "
                "JOIN parking_slots ps ON ps.slot_id = a.slot_id "
                "JOIN parking_areas pa ON pa.area_id = ps.area_id "
                "LEFT JOIN parking_rates pr ON pr.area_id = pa.area_id "
                "AND pr.vehicle_type = v.vehicle_type AND pr.slot_type = ps.slot_type "
                "WHERE a.vehicle_id = $1 AND a.user_id = $2 AND a.checkout_at IS NULL",
                pqxx::params{vehicle_id, user->user_id});

            if (assign_rows.empty())
            {
                return api::errorResponse(404, "ACTIVE_PARKING_NOT_FOUND", "This vehicle is not currently parked.");
            }

            const auto &row = assign_rows[0];
            const std::int64_t start_epoch = row["start_epoch"].as<std::int64_t>();
            const std::int64_t planned_end_epoch = row["planned_end_epoch"].as<std::int64_t>();
            const std::int64_t exit_epoch = row["exit_epoch"].as<std::int64_t>();
            const double hourly_rate = row["hourly_rate"].as<double>();
            const auto fee_calc = api::calculateParkingFee(start_epoch, planned_end_epoch, exit_epoch, hourly_rate);

            api::SessionFeeSummaryData summary;
            summary.session_id = 0;
            summary.reservation_id = 0;
            summary.vehicle_id = static_cast<std::int64_t>(vehicle_id);
            summary.registration_number = row["registration_number"].as<std::string>();
            summary.vehicle_type = row["vehicle_type"].as<std::string>();
            summary.slot_id = row["slot_id"].as<std::int64_t>();
            summary.slot_number = row["slot_number"].as<std::string>();
            summary.area_name = row["area_name"].as<std::string>();
            summary.location = row["location"].as<std::string>();
            summary.entry_time = row["entry_time_str"].as<std::string>();
            summary.exit_time = row["exit_time_str"].as<std::string>();
            summary.reserved_end_time = row["planned_end_time_str"].as<std::string>();
            summary.planned_end_time = row["planned_end_time_str"].as<std::string>();
            summary.duration_minutes = fee_calc.duration_minutes;
            summary.booked_hours = fee_calc.booked_hours;
            summary.billed_hours = fee_calc.booked_hours;
            summary.overstay_duration_minutes = fee_calc.overstay_duration_minutes;
            summary.billed_overstay_hours = fee_calc.overstay_duration_minutes > 0 ? (fee_calc.overstay_duration_minutes + 59) / 60 : 0;
            summary.grace_period_minutes = fee_calc.grace_period_minutes;
            summary.billed_overstay_minutes = fee_calc.billed_overstay_minutes;
            summary.billed_overtime_minutes = fee_calc.billed_overstay_minutes;
            summary.hourly_rate = hourly_rate;
            summary.overstay_rate = row["overstay_rate"].as<double>();
            summary.overtime_rate_per_minute = fee_calc.overtime_rate_per_minute;
            summary.base_fee = fee_calc.base_fee;
            summary.overstay_fee = fee_calc.overstay_fee;
            summary.total_fee = fee_calc.total_fee;
            summary.payment_status = "unpaid";
            summary.session_status = fee_calc.overstay_duration_minutes > 0 ? "overstayed" : "active";

            crow::json::wvalue response;
            response["session"] = api::sessionFeeSummaryJson(summary);
            return crow::response(200, response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Vehicle fee summary failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The vehicle fee summary could not be calculated.");
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

            std::string payment_action = "payment_at_desk";
            if (!request.body.empty())
            {
                const auto body = crow::json::load(request.body);
                if (body && body.has("payment_action") && body["payment_action"].t() == crow::json::type::String)
                {
                    payment_action = body["payment_action"].s();
                }
            }

            // If parked via session, delegate to exitParkingSession
            const pqxx::result session_rows = database.executeRead(
                "SELECT session_id FROM parking_sessions "
                "WHERE vehicle_id = $1 AND user_id = $2 AND exit_time IS NULL AND status IN ('active', 'overstayed')",
                pqxx::params{vehicle_id, user->user_id});

            if (!session_rows.empty())
            {
                const std::int64_t session_id = session_rows[0]["session_id"].as<std::int64_t>();
                const SessionExitResult exit_res = database.exitParkingSession(user->user_id, session_id, payment_action);
                if (exit_res.status == SessionExitStatus::completed)
                {
                    crow::json::wvalue response;
                    response["message"] = "Vehicle checked out and slot released.";
                    response["session"] = api::sessionFeeSummaryJson(exit_res.fee_summary);
                    response["slot_id"] = exit_res.fee_summary.slot_id;
                    return crow::response(200, response);
                }
            }

            // Otherwise checkout from direct parking_assignments
            const pqxx::result assign_rows = database.executeRead(
                "SELECT a.assignment_id, a.slot_id, ps.slot_number, ps.slot_type, ps.area_id, "
                "pa.name AS area_name, pa.location, "
                "v.registration_number, v.vehicle_type, "
                "to_char(a.parked_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS entry_time_str, "
                "to_char(clock_timestamp() AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS exit_time_str, "
                "to_char(COALESCE(a.planned_end_time, a.parked_at + INTERVAL '2 hours') AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS planned_end_time_str, "
                "EXTRACT(EPOCH FROM a.parked_at)::BIGINT AS start_epoch, "
                "EXTRACT(EPOCH FROM COALESCE(a.planned_end_time, a.parked_at + INTERVAL '2 hours'))::BIGINT AS planned_end_epoch, "
                "EXTRACT(EPOCH FROM clock_timestamp())::BIGINT AS exit_epoch, "
                "COALESCE(pr.hourly_rate, 40.00) AS hourly_rate, "
                "COALESCE(pr.overstay_rate, 60.00) AS overstay_rate "
                "FROM parking_assignments a "
                "JOIN vehicles v ON v.vehicle_id = a.vehicle_id "
                "JOIN parking_slots ps ON ps.slot_id = a.slot_id "
                "JOIN parking_areas pa ON pa.area_id = ps.area_id "
                "LEFT JOIN parking_rates pr ON pr.area_id = pa.area_id "
                "AND pr.vehicle_type = v.vehicle_type AND pr.slot_type = ps.slot_type "
                "WHERE a.vehicle_id = $1 AND a.user_id = $2 AND a.checkout_at IS NULL",
                pqxx::params{vehicle_id, user->user_id});

            if (assign_rows.empty())
            {
                return api::errorResponse(404, "ACTIVE_PARKING_NOT_FOUND", "This vehicle has no active parking assignment.");
            }

            const auto &row = assign_rows[0];
            const std::int64_t assignment_id = row["assignment_id"].as<std::int64_t>();
            const std::int64_t slot_id = row["slot_id"].as<std::int64_t>();
            const std::int64_t area_id = row["area_id"].as<std::int64_t>();
            const std::string exit_time_str = row["exit_time_str"].as<std::string>();
            const std::int64_t start_epoch = row["start_epoch"].as<std::int64_t>();
            const std::int64_t planned_end_epoch = row["planned_end_epoch"].as<std::int64_t>();
            const std::int64_t exit_epoch = row["exit_epoch"].as<std::int64_t>();
            const double hourly_rate = row["hourly_rate"].as<double>();
            const auto fee_calc = api::calculateParkingFee(start_epoch, planned_end_epoch, exit_epoch, hourly_rate);

            api::SessionFeeSummaryData summary;
            summary.session_id = 0;
            summary.reservation_id = 0;
            summary.vehicle_id = static_cast<std::int64_t>(vehicle_id);
            summary.registration_number = row["registration_number"].as<std::string>();
            summary.vehicle_type = row["vehicle_type"].as<std::string>();
            summary.slot_id = slot_id;
            summary.slot_number = row["slot_number"].as<std::string>();
            summary.area_name = row["area_name"].as<std::string>();
            summary.location = row["location"].as<std::string>();
            summary.entry_time = row["entry_time_str"].as<std::string>();
            summary.exit_time = exit_time_str;
            summary.reserved_end_time = row["planned_end_time_str"].as<std::string>();
            summary.planned_end_time = row["planned_end_time_str"].as<std::string>();
            summary.duration_minutes = fee_calc.duration_minutes;
            summary.booked_hours = fee_calc.booked_hours;
            summary.billed_hours = fee_calc.booked_hours;
            summary.overstay_duration_minutes = fee_calc.overstay_duration_minutes;
            summary.billed_overstay_hours = fee_calc.overstay_duration_minutes > 0 ? (fee_calc.overstay_duration_minutes + 59) / 60 : 0;
            summary.grace_period_minutes = fee_calc.grace_period_minutes;
            summary.billed_overstay_minutes = fee_calc.billed_overstay_minutes;
            summary.billed_overtime_minutes = fee_calc.billed_overstay_minutes;
            summary.hourly_rate = hourly_rate;
            summary.overstay_rate = row["overstay_rate"].as<double>();
            summary.overtime_rate_per_minute = fee_calc.overtime_rate_per_minute;
            summary.base_fee = fee_calc.base_fee;
            summary.overstay_fee = fee_calc.overstay_fee;
            summary.total_fee = fee_calc.total_fee;
            summary.payment_status = payment_action;
            summary.session_status = "completed";

            std::string payment_db_status = "successful";
            std::string payment_method = (payment_action == "online_payment") ? "UPI" : "CASH";
            std::string tx_ref = (payment_action == "online_payment" ? "ONLINE-DIRECT-" : "DESK-DIRECT-") + std::to_string(assignment_id) + "-" + std::to_string(exit_epoch);

            pqxx::connection connection = database.createConnection();
            pqxx::work transaction(connection);

            transaction.exec_params(
                "UPDATE parking_assignments SET checkout_at = CURRENT_TIMESTAMP "
                "WHERE vehicle_id = $1 AND user_id = $2 AND checkout_at IS NULL",
                vehicle_id, user->user_id);

            transaction.exec_params(
                "UPDATE parking_slots ps SET status = CASE "
                "WHEN EXISTS (SELECT 1 FROM parking_sessions s "
                "WHERE s.slot_id = ps.slot_id AND s.exit_time IS NULL "
                "AND s.status IN ('active', 'overstayed')) THEN 'occupied' "
                "WHEN EXISTS (SELECT 1 FROM reservations r "
                "WHERE r.slot_id = ps.slot_id AND r.status IN ('pending', 'confirmed', 'active') "
                "AND r.start_time <= CURRENT_TIMESTAMP AND r.end_time > CURRENT_TIMESTAMP) THEN 'reserved' "
                "ELSE 'available' END "
                "WHERE ps.slot_id = $1",
                slot_id);

            transaction.exec_params(
                "INSERT INTO payments "
                "(reservation_id, session_id, user_id, amount, payment_method, status, transaction_reference, paid_at) "
                "VALUES (NULL, NULL, $1, $2, $3, $4, $5, CURRENT_TIMESTAMP)",
                user->user_id, summary.total_fee, payment_method, payment_db_status, tx_ref);

            transaction.exec_params(
                "INSERT INTO notifications (user_id, title, message, type) "
                "VALUES ($1, 'Direct parking complete', "
                "'Parking ended. Final fee: INR ' || to_char($2::numeric, 'FM999999990.00') || '.', "
                "'exit')",
                user->user_id, summary.total_fee);

            if (fee_calc.overstay_duration_minutes > 0)
            {
                transaction.exec_params(
                    "INSERT INTO notifications (user_id, title, message, type) "
                    "VALUES ($1, 'Parking overstay recorded', "
                    "'Parking exceeded planned end time by ' || $2::text || ' minutes.', "
                    "'overstay')",
                    user->user_id, fee_calc.overstay_duration_minutes);
            }

            transaction.commit();

            crow::json::wvalue response;
            response["message"] = "Vehicle checked out and slot released.";
            response["slot_id"] = slot_id;
            response["area_id"] = area_id;
            response["checkout_at"] = exit_time_str;
            response["session"] = api::sessionFeeSummaryJson(summary);
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Vehicle checkout failed: " << exception.what() << std::endl;
            return api::errorResponse(500, "CHECKOUT_FAILED", std::string("Vehicle checkout failed: ") + exception.what());
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
            const auto needs_accessible_parking = jsonBoolean(body, "needs_accessible_parking");
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
            if (!needs_accessible_parking && body.has("needs_accessible_parking"))
            {
                return api::errorResponse(400, "INVALID_ACCESSIBLE_PARKING_FLAG", "needs_accessible_parking must be a boolean.");
            }

            const pqxx::result rows = database.executeWrite(
                "INSERT INTO vehicles "
                "(user_id, registration_number, vehicle_type, is_electric, needs_accessible_parking) "
                "VALUES ($1, $2, $3, $4, $5) "
                "RETURNING vehicle_id, registration_number, vehicle_type, is_electric, needs_accessible_parking",
                pqxx::params{user->user_id, registration, vehicle_type, *is_electric, needs_accessible_parking.value_or(false)});
            crow::json::wvalue response;
            response["vehicle"] = api::vehicleJson(
                rows[0]["vehicle_id"].as<std::int64_t>(),
                rows[0]["registration_number"].c_str(),
                rows[0]["vehicle_type"].c_str(),
                rows[0]["is_electric"].as<bool>(),
                rows[0]["needs_accessible_parking"].as<bool>());
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
            const auto needs_accessible_parking = jsonBoolean(body, "needs_accessible_parking");
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
            if (!needs_accessible_parking && body.has("needs_accessible_parking"))
            {
                return api::errorResponse(400, "INVALID_ACCESSIBLE_PARKING_FLAG", "needs_accessible_parking must be a boolean.");
            }

            const pqxx::result rows = database.executeWrite(
                "UPDATE vehicles SET registration_number = $1, vehicle_type = $2, is_electric = $3, "
                "needs_accessible_parking = CASE WHEN $4 THEN $5 ELSE needs_accessible_parking END "
                "WHERE vehicle_id = $6 AND user_id = $7 "
                "AND NOT EXISTS (SELECT 1 FROM parking_assignments a "
                "WHERE a.vehicle_id = vehicles.vehicle_id AND a.checkout_at IS NULL) "
                "AND NOT EXISTS (SELECT 1 FROM parking_sessions s "
                "WHERE s.vehicle_id = vehicles.vehicle_id AND s.exit_time IS NULL "
                "AND s.status IN ('active', 'overstayed')) "
                "RETURNING vehicle_id, registration_number, vehicle_type, is_electric, needs_accessible_parking",
                pqxx::params{registration, vehicle_type, *is_electric, needs_accessible_parking.has_value(),
                             needs_accessible_parking.value_or(false), vehicle_id, user->user_id});
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
                rows[0]["is_electric"].as<bool>(),
                rows[0]["needs_accessible_parking"].as<bool>());
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
