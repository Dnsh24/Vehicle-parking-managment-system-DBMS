#include "database.h"
#include "../api/fee_calculator.h"

#include <exception>
#include <string>
#include <algorithm>

namespace
{
std::string quoteConnectionValue(const std::string &value)
{
    std::string quoted_value = "'";
    for (const char character : value)
    {
        if (character == '\\' || character == '\'')
        {
            quoted_value += '\\';
        }
        quoted_value += character;
    }
    quoted_value += '\'';
    return quoted_value;
}
}

Database::Database(const BackendConfig &config)
    : config_(config)
{
}

pqxx::connection Database::createConnection() const
{
    return pqxx::connection(
        "host=" + quoteConnectionValue(config_.database_host) +
        " port=" + quoteConnectionValue(config_.database_port) +
        " dbname=" + quoteConnectionValue(config_.database_name) +
        " user=" + quoteConnectionValue(config_.database_user) +
        " password=" + quoteConnectionValue(config_.database_password));
}

DatabaseCheckResult Database::checkConnection() const
{
    try
    {
        pqxx::connection connection = createConnection();
        pqxx::work transaction(connection);
        transaction.exec("SELECT 1");
        try
        {
            transaction.exec0("ALTER TABLE payments ALTER COLUMN reservation_id DROP NOT NULL");
        }
        catch (...)
        {
        }
        transaction.commit();
        return {true, {}};
    }
    catch (const std::exception &exception)
    {
        return {false, exception.what()};
    }
}

pqxx::result Database::executeRead(
    const std::string &query,
    const pqxx::params &parameters) const
{
    pqxx::connection connection = createConnection();
    pqxx::read_transaction transaction(connection);
    return transaction.exec_params(query, parameters);
}

pqxx::result Database::executeWrite(
    const std::string &query,
    const pqxx::params &parameters) const
{
    pqxx::connection connection = createConnection();
    pqxx::work transaction(connection);
    const pqxx::result result = transaction.exec_params(query, parameters);
    transaction.commit();
    return result;
}

VehicleDeletionResult Database::deleteVehicleAndHistory(
    std::int64_t vehicle_id,
    std::int64_t user_id) const
{
    pqxx::connection connection = createConnection();
    pqxx::work transaction(connection);

    const pqxx::result vehicle = transaction.exec_params(
        "SELECT vehicle_id FROM vehicles "
        "WHERE vehicle_id = $1 AND user_id = $2 FOR UPDATE",
        vehicle_id,
        user_id);
    if (vehicle.empty())
    {
        return VehicleDeletionResult::not_found;
    }

    const pqxx::result active_parking = transaction.exec_params(
        "SELECT EXISTS (SELECT 1 FROM parking_assignments "
        "WHERE vehicle_id = $1 AND checkout_at IS NULL) "
        "OR EXISTS (SELECT 1 FROM parking_sessions "
        "WHERE vehicle_id = $1 AND exit_time IS NULL "
        "AND status IN ('active', 'overstayed')) AS is_parked",
        vehicle_id);
    if (active_parking[0]["is_parked"].as<bool>())
    {
        return VehicleDeletionResult::currently_parked;
    }

    transaction.exec_params(
        "DELETE FROM payments "
        "WHERE reservation_id IN (SELECT reservation_id FROM reservations WHERE vehicle_id = $1) "
        "OR session_id IN (SELECT session_id FROM parking_sessions WHERE vehicle_id = $1)",
        vehicle_id);
    transaction.exec_params(
        "DELETE FROM parking_sessions WHERE vehicle_id = $1",
        vehicle_id);
    transaction.exec_params(
        "DELETE FROM parking_assignments WHERE vehicle_id = $1",
        vehicle_id);
    transaction.exec_params(
        "DELETE FROM reservations WHERE vehicle_id = $1",
        vehicle_id);
    transaction.exec_params(
        "DELETE FROM vehicles WHERE vehicle_id = $1 AND user_id = $2",
        vehicle_id,
        user_id);
    transaction.commit();
    return VehicleDeletionResult::deleted;
}

UserDeletionResult Database::deleteUserAndHistory(
    std::int64_t target_user_id) const
{
    pqxx::connection connection = createConnection();
    pqxx::work transaction(connection);

    const pqxx::result user_rows = transaction.exec_params(
        "SELECT user_id FROM users WHERE user_id = $1 FOR UPDATE",
        target_user_id);
    if (user_rows.empty())
    {
        return UserDeletionResult::not_found;
    }

    const pqxx::result active_assignment = transaction.exec_params(
        "SELECT 1 FROM parking_assignments WHERE user_id = $1 AND checkout_at IS NULL LIMIT 1",
        target_user_id);
    const pqxx::result active_session = transaction.exec_params(
        "SELECT 1 FROM parking_sessions WHERE user_id = $1 AND exit_time IS NULL "
        "AND status IN ('active', 'overstayed') LIMIT 1",
        target_user_id);
    if (!active_assignment.empty() || !active_session.empty())
    {
        return UserDeletionResult::active_parking;
    }

    const pqxx::result future_res = transaction.exec_params(
        "SELECT 1 FROM reservations WHERE user_id = $1 AND status IN ('pending', 'confirmed', 'active') "
        "AND end_time > CURRENT_TIMESTAMP LIMIT 1",
        target_user_id);
    if (!future_res.empty())
    {
        return UserDeletionResult::future_reservations;
    }

    transaction.exec_params("DELETE FROM payments WHERE user_id = $1", target_user_id);
    transaction.exec_params("DELETE FROM parking_sessions WHERE user_id = $1", target_user_id);
    transaction.exec_params("DELETE FROM reservations WHERE user_id = $1", target_user_id);
    transaction.exec_params("DELETE FROM parking_assignments WHERE user_id = $1", target_user_id);
    transaction.exec_params("DELETE FROM vehicles WHERE user_id = $1", target_user_id);
    transaction.exec_params("DELETE FROM notifications WHERE user_id = $1", target_user_id);
    transaction.exec_params("DELETE FROM auth_sessions WHERE user_id = $1", target_user_id);
    transaction.exec_params("DELETE FROM users WHERE user_id = $1", target_user_id);

    transaction.commit();
    return UserDeletionResult::deleted;
}


ReservationCreationResult Database::createReservation(
    std::int64_t user_id,
    std::int64_t vehicle_id,
    std::int64_t slot_id,
    const std::string &start_time,
    const std::string &end_time) const
{
    pqxx::connection connection = createConnection();
    pqxx::work transaction(connection);

    const pqxx::result vehicle = transaction.exec_params(
        "SELECT vehicle_type, is_electric, needs_accessible_parking FROM vehicles "
        "WHERE vehicle_id = $1 AND user_id = $2 FOR UPDATE",
        vehicle_id,
        user_id);
    if (vehicle.empty())
    {
        return {ReservationCreationStatus::vehicle_not_found};
    }

    const pqxx::result valid_range = transaction.exec_params(
        "SELECT $1::timestamptz > CURRENT_TIMESTAMP "
        "AND $2::timestamptz > $1::timestamptz AS valid",
        start_time,
        end_time);
    if (!valid_range[0]["valid"].as<bool>())
    {
        return {ReservationCreationStatus::invalid_time_range};
    }

    const pqxx::result slot = transaction.exec_params(
        "SELECT ps.slot_id, ps.vehicle_type, "
        "(ps.slot_type ILIKE '%handicap%' OR ps.slot_type ILIKE '%accessible%') AS is_accessible, "
        "(ps.is_ev OR ps.slot_type ~* '(^|[^[:alnum:]])ev([^[:alnum:]]|$)' OR ps.slot_type ILIKE '%electric%' "
        "OR ps.slot_type ILIKE '%charg%') AS is_ev, ps.status, pa.status AS area_status "
        "FROM parking_slots ps "
        "JOIN parking_areas pa ON pa.area_id = ps.area_id "
        "WHERE ps.slot_id = $1 FOR UPDATE OF ps",
        slot_id);
    if (slot.empty() ||
        slot[0]["status"].as<std::string>() != "available" ||
        slot[0]["area_status"].as<std::string>() != "active" ||
        slot[0]["vehicle_type"].as<std::string>() !=
            vehicle[0]["vehicle_type"].as<std::string>() ||
        (slot[0]["is_ev"].as<bool>() && !vehicle[0]["is_electric"].as<bool>()) ||
        (slot[0]["is_accessible"].as<bool>() &&
         !vehicle[0]["needs_accessible_parking"].as<bool>()))
    {
        return {ReservationCreationStatus::slot_unavailable};
    }

    const pqxx::result vehicle_conflict = transaction.exec_params(
        "SELECT EXISTS ("
        "SELECT 1 FROM reservations r "
        "WHERE r.vehicle_id = $1 AND r.status IN ('pending', 'confirmed', 'active') "
        "AND r.start_time < $3::timestamptz AND r.end_time > $2::timestamptz"
        ") OR EXISTS ("
        "SELECT 1 FROM parking_assignments a "
        "WHERE a.vehicle_id = $1 AND a.checkout_at IS NULL"
        ") OR EXISTS ("
        "SELECT 1 FROM parking_sessions s "
        "WHERE s.vehicle_id = $1 AND s.exit_time IS NULL "
        "AND s.status IN ('active', 'overstayed')"
        ") AS unavailable",
        vehicle_id,
        start_time,
        end_time);
    if (vehicle_conflict[0]["unavailable"].as<bool>())
    {
        return {ReservationCreationStatus::vehicle_unavailable};
    }

    const pqxx::result slot_conflict = transaction.exec_params(
        "SELECT EXISTS ("
        "SELECT 1 FROM reservations r "
        "WHERE r.slot_id = $1 AND r.status IN ('pending', 'confirmed', 'active') "
        "AND r.start_time < $3::timestamptz AND r.end_time > $2::timestamptz"
        ") OR EXISTS ("
        "SELECT 1 FROM parking_assignments a "
        "WHERE a.slot_id = $1 AND a.checkout_at IS NULL"
        ") OR EXISTS ("
        "SELECT 1 FROM parking_sessions s "
        "WHERE s.slot_id = $1 AND s.exit_time IS NULL "
        "AND s.status IN ('active', 'overstayed')"
        ") OR EXISTS ("
        "SELECT 1 FROM maintenance m "
        "WHERE m.slot_id = $1 AND m.status IN ('scheduled', 'in_progress') "
        "AND m.start_time < $3::timestamptz "
        "AND (m.end_time IS NULL OR m.end_time > $2::timestamptz)"
        ") AS unavailable",
        slot_id,
        start_time,
        end_time);
    if (slot_conflict[0]["unavailable"].as<bool>())
    {
        return {ReservationCreationStatus::slot_unavailable};
    }

    const pqxx::result inserted = transaction.exec_params(
        "INSERT INTO reservations (user_id, vehicle_id, slot_id, start_time, end_time, status) "
        "VALUES ($1, $2, $3, $4::timestamptz, $5::timestamptz, 'confirmed') "
        "RETURNING reservation_id",
        user_id,
        vehicle_id,
        slot_id,
        start_time,
        end_time);
    const std::int64_t reservation_id = inserted[0]["reservation_id"].as<std::int64_t>();
    transaction.exec_params(
        "INSERT INTO notifications (user_id, title, message, type) "
        "VALUES ($1, 'Reservation confirmed', "
        "'Your reservation #' || $2::text || ' is confirmed.', 'reservation_confirmed')",
        user_id,
        reservation_id);
    transaction.commit();
    return {
        ReservationCreationStatus::created,
        reservation_id};
}

ReservationCheckInResult Database::checkInReservation(
    std::int64_t user_id,
    std::int64_t reservation_id) const
{
    pqxx::connection connection = createConnection();
    pqxx::work transaction(connection);

    const pqxx::result reservation = transaction.exec_params(
        "SELECT vehicle_id, slot_id, start_time, end_time, status "
        "FROM reservations WHERE reservation_id = $1 AND user_id = $2 FOR UPDATE",
        reservation_id,
        user_id);
    if (reservation.empty())
    {
        return {ReservationCheckInStatus::not_found};
    }
    if (reservation[0]["status"].as<std::string>() != "confirmed")
    {
        if (reservation[0]["status"].as<std::string>() == "cancelled")
        {
            return {ReservationCheckInStatus::grace_period_exceeded};
        }
        return {ReservationCheckInStatus::not_confirmed};
    }

    const std::int64_t vehicle_id = reservation[0]["vehicle_id"].as<std::int64_t>();
    const std::int64_t slot_id = reservation[0]["slot_id"].as<std::int64_t>();
    const pqxx::result vehicle = transaction.exec_params(
        "SELECT needs_accessible_parking FROM vehicles WHERE vehicle_id = $1 FOR UPDATE",
        vehicle_id);
    if (vehicle.empty())
    {
        return {ReservationCheckInStatus::vehicle_unavailable};
    }

    const pqxx::result slot = transaction.exec_params(
        "SELECT ps.status, "
        "(ps.slot_type ILIKE '%handicap%' OR ps.slot_type ILIKE '%accessible%') AS is_accessible, "
        "pa.status AS area_status "
        "FROM parking_slots ps "
        "JOIN parking_areas pa ON pa.area_id = ps.area_id "
        "WHERE ps.slot_id = $1 FOR UPDATE OF ps",
        slot_id);
    if (slot.empty() ||
        (slot[0]["status"].as<std::string>() != "available" &&
         slot[0]["status"].as<std::string>() != "reserved") ||
        slot[0]["area_status"].as<std::string>() != "active" ||
        (slot[0]["is_accessible"].as<bool>() &&
         !vehicle[0]["needs_accessible_parking"].as<bool>()))
    {
        return {ReservationCheckInStatus::slot_unavailable};
    }

    const pqxx::result window = transaction.exec_params(
        "SELECT statement_timestamp() < $1::timestamptz AS too_early, "
        "statement_timestamp() > ($1::timestamptz + interval '5 minutes') AS grace_exceeded, "
        "statement_timestamp() >= $2::timestamptz AS expired",
        reservation[0]["start_time"].as<std::string>(),
        reservation[0]["end_time"].as<std::string>());
    if (window[0]["too_early"].as<bool>())
    {
        return {ReservationCheckInStatus::too_early};
    }
    if (window[0]["grace_exceeded"].as<bool>() || window[0]["expired"].as<bool>())
    {
        transaction.exec_params(
            "UPDATE reservations SET status = 'cancelled' "
            "WHERE reservation_id = $1 AND status = 'confirmed'",
            reservation_id);
        transaction.exec_params(
            "INSERT INTO notifications (user_id, title, message, type) "
            "VALUES ($1, 'Reservation cancelled', "
            "'Reservation #' || $2::text || ' was cancelled because check-in was not completed within the 5-minute grace period.', "
            "'reservation_cancelled')",
            user_id,
            reservation_id);
        transaction.commit();
        return {ReservationCheckInStatus::grace_period_exceeded};
    }

    const pqxx::result conflicts = transaction.exec_params(
        "SELECT EXISTS (SELECT 1 FROM parking_assignments "
        "WHERE slot_id = $1 AND checkout_at IS NULL) "
        "OR EXISTS (SELECT 1 FROM parking_sessions "
        "WHERE slot_id = $1 AND exit_time IS NULL "
        "AND status IN ('active', 'overstayed')) AS slot_unavailable, "
        "EXISTS (SELECT 1 FROM parking_assignments "
        "WHERE vehicle_id = $2 AND checkout_at IS NULL) "
        "OR EXISTS (SELECT 1 FROM parking_sessions "
        "WHERE vehicle_id = $2 AND exit_time IS NULL "
        "AND status IN ('active', 'overstayed')) AS vehicle_unavailable",
        slot_id,
        vehicle_id);
    if (conflicts[0]["vehicle_unavailable"].as<bool>())
    {
        return {ReservationCheckInStatus::vehicle_unavailable};
    }
    if (conflicts[0]["slot_unavailable"].as<bool>())
    {
        return {ReservationCheckInStatus::slot_unavailable};
    }

    const pqxx::result maintenance = transaction.exec_params(
        "SELECT EXISTS (SELECT 1 FROM maintenance "
        "WHERE slot_id = $1 AND status IN ('scheduled', 'in_progress') "
        "AND start_time < $2::timestamptz "
        "AND (end_time IS NULL OR end_time > statement_timestamp())) AS unavailable",
        slot_id,
        reservation[0]["end_time"].as<std::string>());
    if (maintenance[0]["unavailable"].as<bool>())
    {
        return {ReservationCheckInStatus::slot_unavailable};
    }

    transaction.exec_params(
        "UPDATE reservations SET status = 'active' "
        "WHERE reservation_id = $1 AND user_id = $2 AND status = 'confirmed'",
        reservation_id,
        user_id);
    transaction.exec_params(
        "UPDATE parking_slots SET status = 'occupied' WHERE slot_id = $1",
        slot_id);
    const pqxx::result session = transaction.exec_params(
        "INSERT INTO parking_sessions "
        "(reservation_id, user_id, vehicle_id, slot_id, entry_time, status) "
        "VALUES ($1, $2, $3, $4, clock_timestamp(), 'active') "
        "RETURNING session_id",
        reservation_id,
        user_id,
        vehicle_id,
        slot_id);
    transaction.exec_params(
        "INSERT INTO notifications (user_id, title, message, type) "
        "VALUES ($1, 'Parking check-in complete', "
        "'Reservation #' || $2::text || ' has been checked in. Session #' || $3::text || ' is active.', "
        "'check_in')",
        user_id,
        reservation_id,
        session[0]["session_id"].as<std::int64_t>());
    transaction.commit();
    return {
        ReservationCheckInStatus::checked_in,
        session[0]["session_id"].as<std::int64_t>()};
}

ReservationCancellationStatus Database::cancelReservation(
    std::int64_t user_id,
    std::int64_t reservation_id) const
{
    pqxx::connection connection = createConnection();
    pqxx::work transaction(connection);

    const pqxx::result reservation = transaction.exec_params(
        "UPDATE reservations SET status = 'cancelled' "
        "WHERE reservation_id = $1 AND user_id = $2 "
        "AND status IN ('pending', 'confirmed') "
        "RETURNING reservation_id",
        reservation_id,
        user_id);
    if (reservation.empty())
    {
        const pqxx::result owned = transaction.exec_params(
            "SELECT reservation_id FROM reservations "
            "WHERE reservation_id = $1 AND user_id = $2",
            reservation_id,
            user_id);
        if (owned.empty())
        {
            return ReservationCancellationStatus::not_found;
        }
        return ReservationCancellationStatus::not_cancellable;
    }

    transaction.exec_params(
        "INSERT INTO notifications (user_id, title, message, type) "
        "VALUES ($1, 'Reservation cancelled', "
        "'Reservation #' || $2::text || ' has been cancelled.', 'reservation_cancelled')",
        user_id,
        reservation_id);
    transaction.commit();
    return ReservationCancellationStatus::cancelled;
}

void Database::cancelOverdueReservations() const
{
    try
    {
        pqxx::connection connection = createConnection();
        pqxx::work transaction(connection);
        transaction.exec0(
            "WITH cancelled_res AS ("
            "  UPDATE reservations "
            "  SET status = 'cancelled' "
            "  WHERE status = 'confirmed' "
            "  AND statement_timestamp() > (start_time + interval '5 minutes') "
            "  RETURNING reservation_id, user_id"
            ") "
            "INSERT INTO notifications (user_id, title, message, type) "
            "SELECT user_id, 'Reservation cancelled', "
            "'Reservation #' || reservation_id::text || ' was cancelled because check-in was not completed within the 5-minute grace period.', "
            "'reservation_cancelled' "
            "FROM cancelled_res");
        transaction.commit();
    }
    catch (const std::exception &e)
    {
        std::cerr << "cancelOverdueReservations error: " << e.what() << std::endl;
    }
}

VehicleParkingResult Database::parkVehicle(
    std::int64_t user_id,
    std::int64_t vehicle_id,
    std::int64_t slot_id,
    int duration_minutes) const
{
    pqxx::connection connection = createConnection();
    pqxx::work transaction(connection);

    const pqxx::result vehicle = transaction.exec_params(
        "SELECT vehicle_type, is_electric, needs_accessible_parking FROM vehicles "
        "WHERE vehicle_id = $1 AND user_id = $2 FOR UPDATE",
        vehicle_id,
        user_id);
    if (vehicle.empty())
    {
        return {VehicleParkingStatus::vehicle_not_found};
    }

    const pqxx::result slot = transaction.exec_params(
        "SELECT ps.slot_id, ps.area_id, ps.slot_number, ps.vehicle_type, "
        "(ps.slot_type ILIKE '%handicap%' OR ps.slot_type ILIKE '%accessible%') AS is_accessible, "
        "(ps.is_ev OR ps.slot_type ~* '(^|[^[:alnum:]])ev([^[:alnum:]]|$)' OR ps.slot_type ILIKE '%electric%' "
        "OR ps.slot_type ILIKE '%charg%') AS is_ev, "
        "ps.status, pa.name AS area_name, pa.status AS area_status "
        "FROM parking_slots ps JOIN parking_areas pa ON pa.area_id = ps.area_id "
        "WHERE ps.slot_id = $1 FOR UPDATE OF ps",
        slot_id);
    if (slot.empty() ||
        (slot[0]["status"].as<std::string>() != "available" &&
         slot[0]["status"].as<std::string>() != "occupied" &&
         slot[0]["status"].as<std::string>() != "reserved") ||
        slot[0]["area_status"].as<std::string>() != "active" ||
        slot[0]["vehicle_type"].as<std::string>() != vehicle[0]["vehicle_type"].as<std::string>() ||
        (slot[0]["is_ev"].as<bool>() && !vehicle[0]["is_electric"].as<bool>()) ||
        (slot[0]["is_accessible"].as<bool>() &&
         !vehicle[0]["needs_accessible_parking"].as<bool>()))
    {
        return {VehicleParkingStatus::slot_unavailable};
    }

    const pqxx::result conflicts = transaction.exec_params(
        "SELECT EXISTS (SELECT 1 FROM parking_assignments a "
        "WHERE a.slot_id = $1 AND a.checkout_at IS NULL) "
        "OR EXISTS (SELECT 1 FROM parking_assignments a "
        "WHERE a.vehicle_id = $2 AND a.checkout_at IS NULL) "
        "OR EXISTS (SELECT 1 FROM parking_sessions s "
        "WHERE (s.slot_id = $1 OR s.vehicle_id = $2) AND s.exit_time IS NULL "
        "AND s.status IN ('active', 'overstayed')) "
        "OR EXISTS (SELECT 1 FROM reservations r "
        "WHERE (r.slot_id = $1 OR r.vehicle_id = $2) "
        "AND r.status IN ('pending', 'confirmed', 'active') "
        "AND r.end_time > CURRENT_TIMESTAMP) "
        "OR EXISTS (SELECT 1 FROM maintenance m "
        "WHERE m.slot_id = $1 AND m.status IN ('scheduled', 'in_progress') "
        "AND (m.end_time IS NULL OR m.end_time > CURRENT_TIMESTAMP)) AS unavailable",
        slot_id,
        vehicle_id);
    if (conflicts[0]["unavailable"].as<bool>())
    {
        return {VehicleParkingStatus::slot_unavailable};
    }

    transaction.exec_params(
        "UPDATE parking_slots SET status = 'occupied' WHERE slot_id = $1",
        slot_id);
    const pqxx::result assignment = transaction.exec_params(
        "INSERT INTO parking_assignments (user_id, vehicle_id, slot_id, planned_end_time) "
        "VALUES ($1, $2, $3, CURRENT_TIMESTAMP + ($4 || ' minutes')::interval) "
        "RETURNING assignment_id, "
        "to_char(parked_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS parked_at, "
        "to_char(planned_end_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS planned_end_time",
        user_id,
        vehicle_id,
        slot_id,
        std::to_string(duration_minutes));
    transaction.commit();
    return {
        VehicleParkingStatus::parked,
        assignment[0]["assignment_id"].as<std::int64_t>(),
        slot[0]["slot_id"].as<std::int64_t>(),
        slot[0]["area_id"].as<std::int64_t>(),
        slot[0]["slot_number"].as<std::string>(),
        slot[0]["area_name"].as<std::string>(),
        assignment[0]["parked_at"].as<std::string>(),
        assignment[0]["planned_end_time"].as<std::string>()};
}
SessionExitResult Database::exitParkingSession(
    std::int64_t user_id,
    std::int64_t session_id,
    const std::string &payment_action) const
{
    pqxx::connection connection = createConnection();
    pqxx::work transaction(connection);

    const pqxx::result session_rows = transaction.exec_params(
        "SELECT s.session_id, s.reservation_id, s.user_id, s.vehicle_id, s.slot_id, "
        "s.status AS session_status, s.exit_time, "
        "s.entry_time, "
        "r.end_time AS reserved_end_time, "
        "to_char(s.entry_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS entry_time_str, "
        "to_char(r.end_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS reserved_end_time_str, "
        "v.registration_number, v.vehicle_type, "
        "ps.slot_number, ps.slot_type, ps.area_id, "
        "pa.name AS area_name, pa.location, "
        "COALESCE(pr.hourly_rate, 40.00) AS hourly_rate, "
        "COALESCE(pr.overstay_rate, 60.00) AS overstay_rate "
        "FROM parking_sessions s "
        "JOIN reservations r ON r.reservation_id = s.reservation_id "
        "JOIN vehicles v ON v.vehicle_id = s.vehicle_id "
        "JOIN parking_slots ps ON ps.slot_id = s.slot_id "
        "JOIN parking_areas pa ON pa.area_id = ps.area_id "
        "LEFT JOIN parking_rates pr ON pr.area_id = pa.area_id "
        "AND pr.vehicle_type = v.vehicle_type AND pr.slot_type = ps.slot_type "
        "WHERE s.session_id = $1 AND s.user_id = $2 "
        "FOR UPDATE OF s, ps, r",
        session_id,
        user_id);

    if (session_rows.empty())
    {
        return {SessionExitStatus::not_found, {}};
    }

    const auto &row = session_rows[0];
    const std::string current_status = row["session_status"].as<std::string>();
    if (!row["exit_time"].is_null() || current_status == "completed" || current_status == "cancelled")
    {
        return {SessionExitStatus::already_completed, {}};
    }

    const pqxx::result calc_rows = transaction.exec_params(
        "SELECT "
        "to_char(clock_timestamp() AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS exit_time_str, "
        "EXTRACT(EPOCH FROM $1::timestamptz)::BIGINT AS start_epoch, "
        "EXTRACT(EPOCH FROM $2::timestamptz)::BIGINT AS reserved_end_epoch, "
        "EXTRACT(EPOCH FROM clock_timestamp())::BIGINT AS exit_epoch",
        row["entry_time"].as<std::string>(),
        row["reserved_end_time"].as<std::string>());

    const auto &calc = calc_rows[0];
    const std::string exit_time_str = calc["exit_time_str"].as<std::string>();
    const std::int64_t start_epoch = calc["start_epoch"].as<std::int64_t>();
    const std::int64_t reserved_end_epoch = calc["reserved_end_epoch"].as<std::int64_t>();
    const std::int64_t exit_epoch = calc["exit_epoch"].as<std::int64_t>();

    const double hourly_rate = row["hourly_rate"].as<double>();
    const double overstay_rate = row["overstay_rate"].as<double>();

    const auto fee_calc = api::calculateParkingFee(start_epoch, reserved_end_epoch, exit_epoch, hourly_rate);

    const int duration_minutes = fee_calc.duration_minutes;
    const int billed_hours = fee_calc.booked_hours;
    const int overstay_duration_minutes = fee_calc.overstay_duration_minutes;
    const int billed_overstay_hours = overstay_duration_minutes > 0 ? (overstay_duration_minutes + 59) / 60 : 0;
    const double base_fee = fee_calc.base_fee;
    const double overstay_fee = fee_calc.overstay_fee;
    const double total_fee = fee_calc.total_fee;

    const std::string final_session_status = (overstay_duration_minutes > 0) ? "overstayed" : "completed";

    std::string payment_status = (payment_action == "online_payment") ? "online_payment" : "paid_manually";
    if (payment_action == "unpaid")
    {
        payment_status = "unpaid";
    }
    else if (payment_action == "waived")
    {
        payment_status = "waived";
    }

    transaction.exec_params(
        "UPDATE parking_sessions "
        "SET exit_time = $1::timestamptz, "
        "    duration_minutes = $2, "
        "    overstay_duration_minutes = $3, "
        "    calculated_fee = $4, "
        "    status = $5 "
        "WHERE session_id = $6",
        exit_time_str,
        duration_minutes,
        overstay_duration_minutes,
        total_fee,
        final_session_status,
        session_id);

    const std::int64_t reservation_id = row["reservation_id"].as<std::int64_t>();
    transaction.exec_params(
        "UPDATE reservations SET status = 'completed' "
        "WHERE reservation_id = $1 AND status IN ('confirmed', 'active')",
        reservation_id);

    const std::int64_t slot_id = row["slot_id"].as<std::int64_t>();
    transaction.exec_params(
        "UPDATE parking_slots ps SET status = CASE "
        "WHEN EXISTS ( "
        "    SELECT 1 FROM maintenance m "
        "    WHERE m.slot_id = ps.slot_id "
        "      AND m.status IN ('scheduled', 'in_progress') "
        "      AND m.start_time <= CURRENT_TIMESTAMP "
        "      AND (m.end_time IS NULL OR m.end_time > CURRENT_TIMESTAMP) "
        ") THEN 'maintenance' "
        "ELSE 'available' END "
        "WHERE ps.slot_id = $1",
        slot_id);

    std::string payment_db_status = "successful";
    std::string payment_method = (payment_status == "online_payment") ? "UPI" : "CASH";
    double payment_amount = total_fee;
    std::string paid_at_val = exit_time_str;
    std::string tx_ref = (payment_status == "online_payment" ? "ONLINE-" : "DESK-") + std::to_string(session_id) + "-" + std::to_string(duration_minutes);

    if (payment_status == "unpaid")
    {
        payment_db_status = "pending";
        paid_at_val = "";
    }
    else if (payment_status == "waived")
    {
        payment_db_status = "successful";
        payment_method = "OTHER";
        payment_amount = 0.00;
        tx_ref = "WAIVED-" + std::to_string(session_id);
    }

    if (paid_at_val.empty())
    {
        transaction.exec_params(
            "INSERT INTO payments "
            "(reservation_id, session_id, user_id, amount, payment_method, status, transaction_reference, paid_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, NULL)",
            reservation_id,
            session_id,
            user_id,
            payment_amount,
            payment_method,
            payment_db_status,
            tx_ref);
    }
    else
    {
        transaction.exec_params(
            "INSERT INTO payments "
            "(reservation_id, session_id, user_id, amount, payment_method, status, transaction_reference, paid_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8::timestamptz)",
            reservation_id,
            session_id,
            user_id,
            payment_amount,
            payment_method,
            payment_db_status,
            tx_ref,
            paid_at_val);
    }

    transaction.exec_params(
        "INSERT INTO notifications (user_id, title, message, type) "
        "VALUES ($1, 'Parking session complete', "
        "'Session #' || $2::text || ' ended. Final fee: INR ' || to_char($3::numeric, 'FM999999990.00') || '.', "
        "'exit')",
        user_id,
        session_id,
        total_fee);
    if (overstay_duration_minutes > 0)
    {
        transaction.exec_params(
            "INSERT INTO notifications (user_id, title, message, type) "
            "VALUES ($1, 'Parking overstay recorded', "
            "'Session #' || $2::text || ' exceeded the reserved end time by ' || $3::text || ' minutes.', "
            "'overstay')",
            user_id,
            session_id,
            overstay_duration_minutes);
    }
    transaction.exec_params(
        "INSERT INTO notifications (user_id, title, message, type) "
        "VALUES ($1, 'Fee status updated', "
        "'Session #' || $2::text || ' fee status: ' || $3::text || '.', 'fee_status')",
        user_id,
        session_id,
        payment_status);

    transaction.commit();

    api::SessionFeeSummaryData summary;
    summary.session_id = session_id;
    summary.reservation_id = reservation_id;
    summary.vehicle_id = row["vehicle_id"].as<std::int64_t>();
    summary.registration_number = row["registration_number"].as<std::string>();
    summary.vehicle_type = row["vehicle_type"].as<std::string>();
    summary.slot_id = slot_id;
    summary.slot_number = row["slot_number"].as<std::string>();
    summary.area_name = row["area_name"].as<std::string>();
    summary.location = row["location"].as<std::string>();
    summary.entry_time = row["entry_time_str"].as<std::string>();
    summary.exit_time = exit_time_str;
    summary.reserved_end_time = row["reserved_end_time_str"].as<std::string>();
    summary.planned_end_time = row["reserved_end_time_str"].as<std::string>();
    summary.duration_minutes = duration_minutes;
    summary.booked_hours = fee_calc.booked_hours;
    summary.billed_hours = fee_calc.booked_hours;
    summary.overstay_duration_minutes = fee_calc.overstay_duration_minutes;
    summary.billed_overstay_hours = billed_overstay_hours;
    summary.grace_period_minutes = fee_calc.grace_period_minutes;
    summary.billed_overstay_minutes = fee_calc.billed_overstay_minutes;
    summary.billed_overtime_minutes = fee_calc.billed_overstay_minutes;
    summary.hourly_rate = hourly_rate;
    summary.overstay_rate = overstay_rate;
    summary.overtime_rate_per_minute = fee_calc.overtime_rate_per_minute;
    summary.base_fee = base_fee;
    summary.overstay_fee = overstay_fee;
    summary.total_fee = total_fee;
    summary.payment_status = payment_status;
    summary.session_status = final_session_status;

    return {SessionExitStatus::completed, summary};
}

std::optional<api::SessionFeeSummaryData> Database::getSessionFeeSummary(
    std::int64_t user_id,
    std::int64_t session_id) const
{
    pqxx::connection connection = createConnection();
    pqxx::work transaction(connection);

    const pqxx::result session_rows = transaction.exec_params(
        "SELECT s.session_id, s.reservation_id, s.user_id, s.vehicle_id, s.slot_id, "
        "s.status AS session_status, "
        "s.duration_minutes, s.overstay_duration_minutes, s.calculated_fee, "
        "to_char(s.entry_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS entry_time_str, "
        "to_char(s.exit_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS exit_time_str, "
        "to_char(r.end_time AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS reserved_end_time_str, "
        "s.entry_time, s.exit_time, r.end_time AS reserved_end_time, "
        "EXTRACT(EPOCH FROM r.start_time)::BIGINT AS start_epoch, "
        "EXTRACT(EPOCH FROM r.end_time)::BIGINT AS reserved_end_epoch, "
        "EXTRACT(EPOCH FROM COALESCE(s.exit_time, clock_timestamp()))::BIGINT AS exit_epoch, "
        "to_char(COALESCE(s.exit_time, clock_timestamp()) AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS calc_exit_time_str, "
        "v.registration_number, v.vehicle_type, "
        "ps.slot_number, ps.slot_type, ps.area_id, "
        "pa.name AS area_name, pa.location, "
        "COALESCE(pr.hourly_rate, 40.00) AS hourly_rate, "
        "COALESCE(pr.overstay_rate, 60.00) AS overstay_rate, "
        "p.status AS payment_db_status, p.payment_method "
        "FROM parking_sessions s "
        "JOIN reservations r ON r.reservation_id = s.reservation_id "
        "JOIN vehicles v ON v.vehicle_id = s.vehicle_id "
        "JOIN parking_slots ps ON ps.slot_id = s.slot_id "
        "JOIN parking_areas pa ON pa.area_id = ps.area_id "
        "LEFT JOIN parking_rates pr ON pr.area_id = pa.area_id "
        "AND pr.vehicle_type = v.vehicle_type AND pr.slot_type = ps.slot_type "
        "LEFT JOIN payments p ON p.session_id = s.session_id "
        "WHERE s.session_id = $1 AND s.user_id = $2",
        session_id,
        user_id);

    if (session_rows.empty())
    {
        return std::nullopt;
    }

    const auto &row = session_rows[0];
    api::SessionFeeSummaryData summary;
    summary.session_id = session_id;
    summary.reservation_id = row["reservation_id"].as<std::int64_t>();
    summary.vehicle_id = row["vehicle_id"].as<std::int64_t>();
    summary.registration_number = row["registration_number"].as<std::string>();
    summary.vehicle_type = row["vehicle_type"].as<std::string>();
    summary.slot_id = row["slot_id"].as<std::int64_t>();
    summary.slot_number = row["slot_number"].as<std::string>();
    summary.area_name = row["area_name"].as<std::string>();
    summary.location = row["location"].as<std::string>();
    summary.entry_time = row["entry_time_str"].as<std::string>();
    summary.reserved_end_time = row["reserved_end_time_str"].as<std::string>();
    summary.hourly_rate = row["hourly_rate"].as<double>();
    summary.overstay_rate = row["overstay_rate"].as<double>();

    const std::int64_t start_epoch = row["start_epoch"].as<std::int64_t>();
    const std::int64_t reserved_end_epoch = row["reserved_end_epoch"].as<std::int64_t>();
    const std::int64_t exit_epoch = row["exit_epoch"].as<std::int64_t>();
    const auto fee_calc = api::calculateParkingFee(start_epoch, reserved_end_epoch, exit_epoch, summary.hourly_rate);

    summary.planned_end_time = summary.reserved_end_time;
    summary.exit_time = row["calc_exit_time_str"].as<std::string>();
    summary.duration_minutes = fee_calc.duration_minutes;
    summary.booked_hours = fee_calc.booked_hours;
    summary.billed_hours = fee_calc.booked_hours;
    summary.overstay_duration_minutes = fee_calc.overstay_duration_minutes;
    summary.billed_overstay_hours = fee_calc.overstay_duration_minutes > 0 ? (fee_calc.overstay_duration_minutes + 59) / 60 : 0;
    summary.grace_period_minutes = fee_calc.grace_period_minutes;
    summary.billed_overstay_minutes = fee_calc.billed_overstay_minutes;
    summary.billed_overtime_minutes = fee_calc.billed_overstay_minutes;
    summary.overtime_rate_per_minute = fee_calc.overtime_rate_per_minute;
    summary.base_fee = fee_calc.base_fee;
    summary.overstay_fee = fee_calc.overstay_fee;
    summary.total_fee = fee_calc.total_fee;

    if (!row["exit_time"].is_null())
    {
        summary.session_status = row["session_status"].as<std::string>();
        if (!row["payment_db_status"].is_null())
        {
            const std::string p_status = row["payment_db_status"].as<std::string>();
            const std::string p_method = row["payment_method"].as<std::string>();
            if (p_status == "successful" && p_method == "OTHER")
            {
                summary.payment_status = "waived";
            }
            else if (p_status == "successful")
            {
                summary.payment_status = "paid_manually";
            }
            else
            {
                summary.payment_status = "unpaid";
            }
        }
        else
        {
            summary.payment_status = "unpaid";
        }
    }
    else
    {
        summary.session_status = (fee_calc.overstay_duration_minutes > 0) ? "overstayed" : "active";
        summary.payment_status = "unpaid";
    }

    return summary;
}
