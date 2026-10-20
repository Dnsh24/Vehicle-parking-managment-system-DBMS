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
