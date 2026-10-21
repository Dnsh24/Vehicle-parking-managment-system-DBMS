#include "database.h"

#include <exception>
#include <string>

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
        "DELETE FROM receipts "
        "WHERE reservation_id IN (SELECT reservation_id FROM reservations WHERE vehicle_id = $1) "
        "OR session_id IN (SELECT session_id FROM parking_sessions WHERE vehicle_id = $1) "
        "OR payment_id IN (SELECT payment_id FROM payments "
        "WHERE reservation_id IN (SELECT reservation_id FROM reservations WHERE vehicle_id = $1) "
        "OR session_id IN (SELECT session_id FROM parking_sessions WHERE vehicle_id = $1))",
        vehicle_id);
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
        "SELECT vehicle_type, is_electric FROM vehicles "
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
        "SELECT ps.slot_id, ps.vehicle_type, ps.is_ev, ps.status, pa.status AS area_status "
        "FROM parking_slots ps "
        "JOIN parking_areas pa ON pa.area_id = ps.area_id "
        "WHERE ps.slot_id = $1 FOR UPDATE OF ps",
        slot_id);
    if (slot.empty() ||
        slot[0]["status"].as<std::string>() != "available" ||
        slot[0]["area_status"].as<std::string>() != "active" ||
        slot[0]["vehicle_type"].as<std::string>() !=
            vehicle[0]["vehicle_type"].as<std::string>() ||
        (slot[0]["is_ev"].as<bool>() && !vehicle[0]["is_electric"].as<bool>()))
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
    transaction.commit();
    return {
        ReservationCreationStatus::created,
        inserted[0]["reservation_id"].as<std::int64_t>()};
}

VehicleParkingResult Database::parkVehicle(
    std::int64_t user_id,
    std::int64_t vehicle_id,
    std::int64_t slot_id) const
{
    pqxx::connection connection = createConnection();
    pqxx::work transaction(connection);

    const pqxx::result vehicle = transaction.exec_params(
        "SELECT vehicle_type, is_electric FROM vehicles "
        "WHERE vehicle_id = $1 AND user_id = $2 FOR UPDATE",
        vehicle_id,
        user_id);
    if (vehicle.empty())
    {
        return {VehicleParkingStatus::vehicle_not_found};
    }

    const pqxx::result slot = transaction.exec_params(
        "SELECT ps.slot_id, ps.area_id, ps.slot_number, ps.vehicle_type, ps.is_ev, "
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
        (slot[0]["is_ev"].as<bool>() && !vehicle[0]["is_electric"].as<bool>()))
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
        "INSERT INTO parking_assignments (user_id, vehicle_id, slot_id) "
        "VALUES ($1, $2, $3) "
        "RETURNING assignment_id, to_char(parked_at AT TIME ZONE 'UTC', "
        "'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS parked_at",
        user_id,
        vehicle_id,
        slot_id);
    transaction.commit();
    return {
        VehicleParkingStatus::parked,
        assignment[0]["assignment_id"].as<std::int64_t>(),
        slot[0]["slot_id"].as<std::int64_t>(),
        slot[0]["area_id"].as<std::int64_t>(),
        slot[0]["slot_number"].as<std::string>(),
        slot[0]["area_name"].as<std::string>(),
        assignment[0]["parked_at"].as<std::string>()};
}
