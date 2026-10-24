#include "reservation_routes.h"
#include "route_helpers.h"
#include "api/errors.h"
#include "api/session_responses.h"
#include "validation/validation.h"

#include <iostream>
#include <charconv>
#include <cstdint>
#include <string>

namespace
{
std::string queryValue(const crow::request &request, const char *key)
{
    const char *value = request.url_params.get(key);
    return value == nullptr ? "" : value;
}

bool parsePagination(
    const crow::request &request,
    std::int64_t &limit,
    std::int64_t &offset)
{
    const auto parseValue = [&request](const char *key, std::int64_t default_value, std::int64_t &value)
    {
        const std::string text = queryValue(request, key);
        if (text.empty())
        {
            value = default_value;
            return true;
        }

        const char *begin = text.data();
        const char *end = begin + text.size();
        const auto result = std::from_chars(begin, end, value);
        return result.ec == std::errc{} && result.ptr == end;
    };

    return parseValue("limit", 20, limit) &&
        parseValue("offset", 0, offset) &&
        limit >= 1 && limit <= 100 && offset >= 0;
}

crow::response invalidTimeRange()
{
    return api::errorResponse(
        400,
        "INVALID_TIME_RANGE",
        "Start time must be in the future and end time must be later than start time.");
}

bool isValidFutureRange(
    const Database &database,
    const std::string &start_time,
    const std::string &end_time)
{
    if (!validation::timestamp(start_time, "start_time").valid ||
        !validation::timestamp(end_time, "end_time").valid)
    {
        return false;
    }

    const pqxx::result rows = database.executeRead(
        "SELECT $1::timestamptz > CURRENT_TIMESTAMP "
        "AND $2::timestamptz > $1::timestamptz AS valid",
        pqxx::params{start_time, end_time});
    return rows[0]["valid"].as<bool>();
}

template <typename Row>
crow::json::wvalue reservationJson(const Row &row)
{
    crow::json::wvalue reservation;
    reservation["reservation_id"] = row["reservation_id"].template as<std::int64_t>();
    reservation["vehicle_id"] = row["vehicle_id"].template as<std::int64_t>();
    reservation["registration_number"] = row["registration_number"].c_str();
    reservation["vehicle_type"] = row["vehicle_type"].c_str();
    reservation["slot_id"] = row["slot_id"].template as<std::int64_t>();
    reservation["slot_number"] = row["slot_number"].c_str();
    reservation["area_id"] = row["area_id"].template as<std::int64_t>();
    reservation["area_name"] = row["area_name"].c_str();
    reservation["location"] = row["location"].c_str();
    reservation["start_time"] = row["start_time"].c_str();
    reservation["end_time"] = row["end_time"].c_str();
    reservation["status"] = row["status"].c_str();
    reservation["created_at"] = row["created_at"].c_str();
    if (!row["hourly_rate"].is_null())
    {
        reservation["hourly_rate"] = row["hourly_rate"].template as<double>();
    }
    return reservation;
}

const std::string reservationDetailsQuery =
    "SELECT r.reservation_id, r.vehicle_id, v.registration_number, v.vehicle_type, "
    "r.slot_id, ps.slot_number, pa.area_id, pa.name AS area_name, pa.location, "
    "to_char(r.start_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS start_time, "
    "to_char(r.end_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS end_time, "
    "r.status, "
    "to_char(r.created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at, "
    "pr.hourly_rate "
    "FROM reservations r "
    "JOIN vehicles v ON v.vehicle_id = r.vehicle_id "
    "JOIN parking_slots ps ON ps.slot_id = r.slot_id "
    "JOIN parking_areas pa ON pa.area_id = ps.area_id "
    "LEFT JOIN parking_rates pr ON pr.area_id = pa.area_id "
    "AND pr.vehicle_type = v.vehicle_type AND pr.slot_type = ps.slot_type ";

const std::string activeSessionDetailsQuery =
    "SELECT s.session_id, s.reservation_id, s.vehicle_id, v.registration_number, "
    "v.vehicle_type, s.slot_id, ps.slot_number, pa.name AS area_name, pa.location, "
    "to_char(s.entry_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS entry_time, "
    "to_char(r.end_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS reserved_end_time, "
    "s.status "
    "FROM parking_sessions s "
    "JOIN reservations r ON r.reservation_id = s.reservation_id "
    "JOIN vehicles v ON v.vehicle_id = s.vehicle_id "
    "JOIN parking_slots ps ON ps.slot_id = s.slot_id "
    "JOIN parking_areas pa ON pa.area_id = ps.area_id ";

template <typename Row>
crow::json::wvalue parkingSessionJson(const Row &row)
{
    crow::json::wvalue session;
    session["session_id"] = row["session_id"].template as<std::int64_t>();
    session["reservation_id"] = row["reservation_id"].template as<std::int64_t>();
    session["vehicle_id"] = row["vehicle_id"].template as<std::int64_t>();
    session["registration_number"] = row["registration_number"].c_str();
    session["vehicle_type"] = row["vehicle_type"].c_str();
    session["slot_id"] = row["slot_id"].template as<std::int64_t>();
    session["slot_number"] = row["slot_number"].c_str();
    session["area_name"] = row["area_name"].c_str();
    session["location"] = row["location"].c_str();
    session["entry_time"] = row["entry_time"].c_str();
    session["reserved_end_time"] = row["reserved_end_time"].c_str();
    session["status"] = row["status"].c_str();
    return session;
}
}

namespace routes
{

void registerReservationRoutes(
    crow::App<crow::CORSHandler> &app,
    Database &database)
{
    CROW_ROUTE(app, "/api/reservations/availability")
        .methods(crow::HTTPMethod::GET)
    ([&database](const crow::request &request)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }

            const std::string vehicle_id_value = queryValue(request, "vehicle_id");
            const std::string start_time = queryValue(request, "start_time");
            const std::string end_time = queryValue(request, "end_time");
            if (!validation::positiveInteger(vehicle_id_value, "vehicle_id").valid ||
                !isValidFutureRange(database, start_time, end_time))
            {
                return api::errorResponse(
                    400,
                    "INVALID_AVAILABILITY_FILTER",
                    "A valid vehicle_id and future ISO-8601 start_time/end_time are required.");
            }

            const std::string area_id_value = queryValue(request, "area_id");
            std::int64_t area_id = 0;
            if (!area_id_value.empty())
            {
                if (!validation::positiveInteger(area_id_value, "area_id").valid)
                {
                    return api::errorResponse(400, "INVALID_AREA_ID", "area_id must be a positive integer.");
                }
                area_id = std::stoll(area_id_value);
            }

            const std::string slot_type = queryValue(request, "slot_type");
            const std::string location = queryValue(request, "location");
            const std::string ev_only_value = queryValue(request, "ev_only");
            if (!ev_only_value.empty() && ev_only_value != "true" && ev_only_value != "false")
            {
                return api::errorResponse(400, "INVALID_EV_FILTER", "ev_only must be true or false.");
            }
            const bool ev_only = ev_only_value == "true";
            const std::int64_t vehicle_id = std::stoll(vehicle_id_value);

            const pqxx::result rows = database.executeRead(
                "SELECT ps.slot_id, ps.area_id, pa.name AS area_name, pa.location, "
                "ps.slot_number, ps.vehicle_type, ps.slot_type, "
                "(ps.is_ev OR ps.slot_type ~* '(^|[^[:alnum:]])ev([^[:alnum:]]|$)' "
                "OR ps.slot_type ILIKE '%electric%' OR ps.slot_type ILIKE '%charg%') AS is_ev, pr.hourly_rate "
                "FROM vehicles v "
                "JOIN parking_slots ps ON ps.vehicle_type = v.vehicle_type "
                "JOIN parking_areas pa ON pa.area_id = ps.area_id "
                "LEFT JOIN parking_rates pr ON pr.area_id = pa.area_id "
                "AND pr.vehicle_type = v.vehicle_type AND pr.slot_type = ps.slot_type "
                "WHERE v.vehicle_id = $1 AND v.user_id = $2 "
                "AND pa.status = 'active' AND ps.status = 'available' "
                "AND (NOT (ps.is_ev OR ps.slot_type ~* '(^|[^[:alnum:]])ev([^[:alnum:]]|$)' "
                "OR ps.slot_type ILIKE '%electric%' OR ps.slot_type ILIKE '%charg%') OR v.is_electric) "
                "AND (v.needs_accessible_parking OR "
                "(ps.slot_type NOT ILIKE '%handicap%' AND ps.slot_type NOT ILIKE '%accessible%')) "
                "AND ($3 = 0 OR ps.area_id = $3) "
                "AND ($4 = '' OR ps.slot_type = $4) "
                "AND ($5 = '' OR pa.name ILIKE '%' || $5 || '%' "
                "OR pa.location ILIKE '%' || $5 || '%') "
                "AND (NOT $8::boolean OR (ps.is_ev OR ps.slot_type ~* '(^|[^[:alnum:]])ev([^[:alnum:]]|$)' "
                "OR ps.slot_type ILIKE '%electric%' OR ps.slot_type ILIKE '%charg%')) "
                "AND NOT EXISTS (SELECT 1 FROM parking_assignments a "
                "WHERE a.slot_id = ps.slot_id AND a.checkout_at IS NULL) "
                "AND NOT EXISTS (SELECT 1 FROM parking_sessions s "
                "WHERE s.slot_id = ps.slot_id AND s.exit_time IS NULL "
                "AND s.status IN ('active', 'overstayed')) "
                "AND NOT EXISTS (SELECT 1 FROM maintenance m "
                "WHERE m.slot_id = ps.slot_id AND m.status IN ('scheduled', 'in_progress') "
                "AND m.start_time < $7::timestamptz "
                "AND (m.end_time IS NULL OR m.end_time > $6::timestamptz)) "
                "AND NOT EXISTS (SELECT 1 FROM parking_assignments a "
                "WHERE a.vehicle_id = v.vehicle_id AND a.checkout_at IS NULL) "
                "AND NOT EXISTS (SELECT 1 FROM parking_sessions s "
                "WHERE s.vehicle_id = v.vehicle_id AND s.exit_time IS NULL "
                "AND s.status IN ('active', 'overstayed')) "
                "AND NOT EXISTS (SELECT 1 FROM reservations r "
                "WHERE r.slot_id = ps.slot_id AND r.status IN ('pending', 'confirmed', 'active') "
                "AND r.start_time < $7::timestamptz AND r.end_time > $6::timestamptz) "
                "AND NOT EXISTS (SELECT 1 FROM reservations r "
                "WHERE r.vehicle_id = v.vehicle_id AND r.status IN ('pending', 'confirmed', 'active') "
                "AND r.start_time < $7::timestamptz AND r.end_time > $6::timestamptz) "
                "ORDER BY pa.name, ps.slot_number",
                pqxx::params{
                    vehicle_id,
                    user->user_id,
                    area_id,
                    slot_type,
                    location,
                    start_time,
                    end_time,
                    ev_only});

            crow::json::wvalue response;
            response["slots"] = crow::json::wvalue::list();
            std::size_t index = 0;
            for (const auto &row : rows)
            {
                crow::json::wvalue slot;
                slot["slot_id"] = row["slot_id"].as<std::int64_t>();
                slot["area_id"] = row["area_id"].as<std::int64_t>();
                slot["area_name"] = row["area_name"].c_str();
                slot["location"] = row["location"].c_str();
                slot["slot_number"] = row["slot_number"].c_str();
                slot["vehicle_type"] = row["vehicle_type"].c_str();
                slot["slot_type"] = row["slot_type"].c_str();
                slot["is_ev"] = row["is_ev"].as<bool>();
                if (!row["hourly_rate"].is_null())
                {
                    slot["hourly_rate"] = row["hourly_rate"].as<double>();
                }
                response["slots"][index++] = std::move(slot);
            }
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Reservation availability search failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "Availability could not be searched.");
        } });

    CROW_ROUTE(app, "/api/reservations").methods(crow::HTTPMethod::GET)
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
                reservationDetailsQuery +
                "WHERE r.user_id = $1 ORDER BY r.start_time DESC, r.reservation_id DESC",
                pqxx::params{user->user_id});
            crow::json::wvalue response;
            response["reservations"] = crow::json::wvalue::list();
            for (std::size_t index = 0; index < rows.size(); ++index)
            {
                response["reservations"][index] = reservationJson(rows[index]);
            }
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Reservation list failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "Reservations could not be loaded.");
        } });

    CROW_ROUTE(app, "/api/reservations").methods(crow::HTTPMethod::POST)
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
            const auto vehicle_id = jsonInteger(body, "vehicle_id");
            const auto slot_id = jsonInteger(body, "slot_id");
            const std::string start_time = jsonString(body, "start_time");
            const std::string end_time = jsonString(body, "end_time");
            if (!vehicle_id || *vehicle_id <= 0 || !slot_id || *slot_id <= 0)
            {
                return api::errorResponse(400, "INVALID_RESERVATION", "Positive vehicle_id and slot_id are required.");
            }
            if (!validation::timestamp(start_time, "start_time").valid ||
                !validation::timestamp(end_time, "end_time").valid)
            {
                return invalidTimeRange();
            }

            const ReservationCreationResult result = database.createReservation(
                user->user_id,
                *vehicle_id,
                *slot_id,
                start_time,
                end_time);
            switch (result.status)
            {
            case ReservationCreationStatus::vehicle_not_found:
                return api::errorResponse(404, "VEHICLE_NOT_FOUND", "The vehicle does not belong to this account.");
            case ReservationCreationStatus::vehicle_unavailable:
                return api::errorResponse(409, "VEHICLE_UNAVAILABLE", "This vehicle has conflicting parking or reservation activity.");
            case ReservationCreationStatus::slot_unavailable:
                return api::errorResponse(409, "PARKING_SLOT_UNAVAILABLE", "The slot is unavailable, incompatible, or already reserved for this time.");
            case ReservationCreationStatus::invalid_time_range:
                return invalidTimeRange();
            case ReservationCreationStatus::created:
                break;
            }

            const pqxx::result rows = database.executeRead(
                reservationDetailsQuery + "WHERE r.reservation_id = $1 AND r.user_id = $2",
                pqxx::params{result.reservation_id, user->user_id});
            crow::json::wvalue response;
            response["reservation"] = reservationJson(rows[0]);
            return crow::response(201, response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Reservation creation failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The reservation could not be created.");
        } });

    CROW_ROUTE(app, "/api/reservations/<uint>/check-in").methods(crow::HTTPMethod::POST)
    ([&database](const crow::request &request, unsigned int reservation_id)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }

            const ReservationCheckInResult result = database.checkInReservation(
                user->user_id,
                static_cast<std::int64_t>(reservation_id));
            switch (result.status)
            {
            case ReservationCheckInStatus::not_found:
                return api::errorResponse(404, "RESERVATION_NOT_FOUND", "The requested reservation does not exist.");
            case ReservationCheckInStatus::not_confirmed:
                return api::errorResponse(409, "RESERVATION_NOT_CHECK_IN_ELIGIBLE", "Only confirmed reservations can be checked in.");
            case ReservationCheckInStatus::too_early:
                return api::errorResponse(409, "CHECK_IN_NOT_OPEN", "Check-in opens at the reservation start time.");
            case ReservationCheckInStatus::expired:
                return api::errorResponse(409, "RESERVATION_EXPIRED", "The reservation end time has passed.");
            case ReservationCheckInStatus::slot_unavailable:
                return api::errorResponse(409, "PARKING_SLOT_UNAVAILABLE", "The reserved slot is unavailable or under maintenance.");
            case ReservationCheckInStatus::vehicle_unavailable:
                return api::errorResponse(409, "VEHICLE_UNAVAILABLE", "The vehicle already has an active parking session or assignment.");
            case ReservationCheckInStatus::checked_in:
                break;
            }

            const pqxx::result rows = database.executeRead(
                activeSessionDetailsQuery + "WHERE s.session_id = $1 AND s.user_id = $2",
                pqxx::params{result.session_id, user->user_id});
            crow::json::wvalue response;
            response["session"] = parkingSessionJson(rows[0]);
            return crow::response(201, response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Reservation check-in failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The reservation could not be checked in.");
        } });

    CROW_ROUTE(app, "/api/parking-sessions/active").methods(crow::HTTPMethod::GET)
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
                activeSessionDetailsQuery +
                "WHERE s.user_id = $1 AND s.exit_time IS NULL AND s.status = 'active' "
                "ORDER BY s.entry_time DESC, s.session_id DESC",
                pqxx::params{user->user_id});
            crow::json::wvalue response;
            response["sessions"] = crow::json::wvalue::list();
            for (std::size_t index = 0; index < rows.size(); ++index)
            {
                response["sessions"][index] = parkingSessionJson(rows[index]);
            }
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Active parking sessions query failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "Active parking sessions could not be loaded.");
        } });

    CROW_ROUTE(app, "/api/history").methods(crow::HTTPMethod::GET)
    ([&database](const crow::request &request)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }

            std::int64_t limit = 20;
            std::int64_t offset = 0;
            if (!parsePagination(request, limit, offset))
            {
                return api::errorResponse(
                    400,
                    "INVALID_PAGINATION",
                    "limit must be between 1 and 100 and offset must be a non-negative integer.");
            }

            const pqxx::result totals = database.executeRead(
                "SELECT COUNT(*) AS total FROM reservations r "
                "LEFT JOIN parking_sessions s ON s.reservation_id = r.reservation_id "
                "WHERE r.user_id = $1 "
                "AND (r.status IN ('completed', 'cancelled', 'expired') OR s.exit_time IS NOT NULL)",
                pqxx::params{user->user_id});
            const pqxx::result rows = database.executeRead(
                "SELECT r.reservation_id, r.status AS reservation_status, "
                "r.vehicle_id, v.registration_number, r.slot_id, ps.slot_number, "
                "pa.name AS area_name, pa.location, "
                "to_char(r.start_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS start_time, "
                "to_char(r.end_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS end_time, "
                "s.session_id, s.status AS session_status, s.duration_minutes, "
                "s.overstay_duration_minutes, s.calculated_fee, "
                "CASE WHEN p.status = 'successful' AND p.payment_method = 'OTHER' THEN 'waived' "
                "WHEN p.status = 'successful' THEN 'paid_manually' ELSE 'unpaid' END AS payment_status, "
                "to_char(s.entry_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS entry_time, "
                "to_char(s.exit_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS exit_time "
                "FROM reservations r "
                "JOIN vehicles v ON v.vehicle_id = r.vehicle_id "
                "JOIN parking_slots ps ON ps.slot_id = r.slot_id "
                "JOIN parking_areas pa ON pa.area_id = ps.area_id "
                "LEFT JOIN parking_sessions s ON s.reservation_id = r.reservation_id "
                "LEFT JOIN LATERAL (SELECT status, payment_method FROM payments "
                "WHERE session_id = s.session_id ORDER BY payment_id DESC LIMIT 1) p ON TRUE "
                "WHERE r.user_id = $1 "
                "AND (r.status IN ('completed', 'cancelled', 'expired') OR s.exit_time IS NOT NULL) "
                "ORDER BY COALESCE(s.exit_time, r.end_time) DESC, r.reservation_id DESC "
                "LIMIT $2 OFFSET $3",
                pqxx::params{user->user_id, limit, offset});

            crow::json::wvalue response;
            response["history"] = crow::json::wvalue::list();
            response["total"] = totals[0]["total"].as<std::int64_t>();
            response["limit"] = limit;
            response["offset"] = offset;
            for (std::size_t index = 0; index < rows.size(); ++index)
            {
                const auto &row = rows[index];
                crow::json::wvalue item;
                item["reservation_id"] = row["reservation_id"].as<std::int64_t>();
                item["reservation_status"] = row["reservation_status"].c_str();
                item["vehicle_id"] = row["vehicle_id"].as<std::int64_t>();
                item["registration_number"] = row["registration_number"].c_str();
                item["slot_id"] = row["slot_id"].as<std::int64_t>();
                item["slot_number"] = row["slot_number"].c_str();
                item["area_name"] = row["area_name"].c_str();
                item["location"] = row["location"].c_str();
                item["start_time"] = row["start_time"].c_str();
                item["end_time"] = row["end_time"].c_str();
                if (!row["session_id"].is_null())
                {
                    crow::json::wvalue fee_summary;
                    fee_summary["session_id"] = row["session_id"].as<std::int64_t>();
                    fee_summary["status"] = row["session_status"].c_str();
                    fee_summary["entry_time"] = row["entry_time"].c_str();
                    fee_summary["exit_time"] = row["exit_time"].c_str();
                    fee_summary["duration_minutes"] = row["duration_minutes"].as<int>();
                    fee_summary["overstay_duration_minutes"] = row["overstay_duration_minutes"].as<int>();
                    fee_summary["total_fee"] = row["calculated_fee"].as<double>();
                    fee_summary["payment_status"] = row["payment_status"].c_str();
                    item["fee_summary"] = std::move(fee_summary);
                }
                response["history"][index] = std::move(item);
            }
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Parking history query failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "Parking history could not be loaded.");
        } });

    CROW_ROUTE(app, "/api/notifications").methods(crow::HTTPMethod::GET)
    ([&database](const crow::request &request)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }

            std::int64_t limit = 20;
            std::int64_t offset = 0;
            if (!parsePagination(request, limit, offset))
            {
                return api::errorResponse(
                    400,
                    "INVALID_PAGINATION",
                    "limit must be between 1 and 100 and offset must be a non-negative integer.");
            }

            const pqxx::result totals = database.executeRead(
                "SELECT COUNT(*) AS total, "
                "COUNT(*) FILTER (WHERE NOT is_read) AS unread_count "
                "FROM notifications WHERE user_id = $1",
                pqxx::params{user->user_id});
            const pqxx::result rows = database.executeRead(
                "SELECT notification_id, title, message, type, is_read, "
                "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at "
                "FROM notifications WHERE user_id = $1 "
                "ORDER BY created_at DESC, notification_id DESC LIMIT $2 OFFSET $3",
                pqxx::params{user->user_id, limit, offset});

            crow::json::wvalue response;
            response["notifications"] = crow::json::wvalue::list();
            response["total"] = totals[0]["total"].as<std::int64_t>();
            response["unread_count"] = totals[0]["unread_count"].as<std::int64_t>();
            response["limit"] = limit;
            response["offset"] = offset;
            for (std::size_t index = 0; index < rows.size(); ++index)
            {
                const auto &row = rows[index];
                crow::json::wvalue notification;
                notification["notification_id"] = row["notification_id"].as<std::int64_t>();
                notification["title"] = row["title"].c_str();
                notification["message"] = row["message"].c_str();
                notification["type"] = row["type"].c_str();
                notification["is_read"] = row["is_read"].as<bool>();
                notification["created_at"] = row["created_at"].c_str();
                response["notifications"][index] = std::move(notification);
            }
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Notification list failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "Notifications could not be loaded.");
        } });

    CROW_ROUTE(app, "/api/notifications/<uint>/read").methods(crow::HTTPMethod::POST)
    ([&database](const crow::request &request, unsigned int notification_id)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }
            const pqxx::result rows = database.executeWrite(
                "UPDATE notifications SET is_read = TRUE "
                "WHERE notification_id = $1 AND user_id = $2 "
                "RETURNING notification_id, is_read",
                pqxx::params{notification_id, user->user_id});
            if (rows.empty())
            {
                return api::errorResponse(
                    404,
                    "NOTIFICATION_NOT_FOUND",
                    "The requested notification does not exist.");
            }
            crow::json::wvalue response;
            response["notification_id"] = rows[0]["notification_id"].as<std::int64_t>();
            response["is_read"] = rows[0]["is_read"].as<bool>();
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Notification update failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The notification could not be updated.");
        } });

    CROW_ROUTE(app, "/api/notifications/read-all").methods(crow::HTTPMethod::POST)
    ([&database](const crow::request &request)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }
            const pqxx::result rows = database.executeWrite(
                "UPDATE notifications SET is_read = TRUE "
                "WHERE user_id = $1 AND NOT is_read RETURNING notification_id",
                pqxx::params{user->user_id});
            crow::json::wvalue response;
            response["updated_count"] = static_cast<std::int64_t>(rows.size());
            response["unread_count"] = 0;
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Notification bulk update failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "Notifications could not be marked as read.");
        } });

    CROW_ROUTE(app, "/api/reservations/<uint>").methods(crow::HTTPMethod::GET)
    ([&database](const crow::request &request, unsigned int reservation_id)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }
            const pqxx::result rows = database.executeRead(
                reservationDetailsQuery +
                "WHERE r.reservation_id = $1 AND r.user_id = $2",
                pqxx::params{reservation_id, user->user_id});
            if (rows.empty())
            {
                return api::errorResponse(404, "RESERVATION_NOT_FOUND", "The requested reservation does not exist.");
            }
            crow::json::wvalue response;
            response["reservation"] = reservationJson(rows[0]);
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Reservation details query failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The reservation could not be loaded.");
        } });

    CROW_ROUTE(app, "/api/reservations/<uint>").methods(crow::HTTPMethod::DELETE)
    ([&database](const crow::request &request, unsigned int reservation_id)
     {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }
            const ReservationCancellationStatus result = database.cancelReservation(
                user->user_id,
                static_cast<std::int64_t>(reservation_id));
            if (result == ReservationCancellationStatus::not_found)
            {
                return api::errorResponse(404, "RESERVATION_NOT_FOUND", "The requested reservation does not exist.");
            }
            if (result == ReservationCancellationStatus::not_cancellable)
            {
                return api::errorResponse(409, "RESERVATION_NOT_CANCELLABLE", "Only pending or confirmed reservations can be cancelled.");
            }
            crow::json::wvalue response;
            response["reservation_id"] = reservation_id;
            response["status"] = "cancelled";
            return crow::response(response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Reservation cancellation failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The reservation could not be cancelled.");
        } });

    auto handleSessionExit = [&database](const crow::request &request, unsigned int session_id)
    {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }

            std::string payment_action = "paid_manually";
            if (!request.body.empty())
            {
                const auto body = crow::json::load(request.body);
                if (body && body.has("payment_action") && body["payment_action"].t() == crow::json::type::String)
                {
                    payment_action = body["payment_action"].s();
                }
            }

            const SessionExitResult result = database.exitParkingSession(
                user->user_id,
                static_cast<std::int64_t>(session_id),
                payment_action);

            switch (result.status)
            {
            case SessionExitStatus::not_found:
                return api::errorResponse(404, "SESSION_NOT_FOUND", "The parking session could not be found.");
            case SessionExitStatus::already_completed:
                return api::errorResponse(409, "SESSION_ALREADY_COMPLETED", "The parking session has already ended.");
            case SessionExitStatus::completed:
                break;
            }

            crow::json::wvalue response;
            response["session"] = api::sessionFeeSummaryJson(result.fee_summary);
            return crow::response(200, response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Session exit failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The parking session exit could not be processed.");
        }
    };

    CROW_ROUTE(app, "/api/parking-sessions/<uint>/exit").methods(crow::HTTPMethod::POST)
    (handleSessionExit);

    CROW_ROUTE(app, "/api/parking-sessions/<uint>/checkout").methods(crow::HTTPMethod::POST)
    (handleSessionExit);

    auto handleSessionDetails = [&database](const crow::request &request, unsigned int session_id)
    {
        try
        {
            const auto user = findAuthenticatedUser(database, request);
            if (!user)
            {
                return unauthorized();
            }

            const auto summary = database.getSessionFeeSummary(
                user->user_id,
                static_cast<std::int64_t>(session_id));

            if (!summary.has_value())
            {
                return api::errorResponse(404, "SESSION_NOT_FOUND", "The parking session could not be found.");
            }

            crow::json::wvalue response;
            response["session"] = api::sessionFeeSummaryJson(*summary);
            return crow::response(200, response);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Session fee summary failed: " << exception.what() << std::endl;
            return api::errorResponse(503, "DATABASE_UNAVAILABLE", "The session fee summary could not be loaded.");
        }
    };

    CROW_ROUTE(app, "/api/parking-sessions/<uint>/fee-summary").methods(crow::HTTPMethod::GET)
    (handleSessionDetails);

    CROW_ROUTE(app, "/api/parking-sessions/<uint>").methods(crow::HTTPMethod::GET)
    (handleSessionDetails);
}

} // namespace routes
