#pragma once

#include "../config/config.h"

#include <pqxx/pqxx>

#include <cstdint>
#include <string>

struct DatabaseCheckResult
{
    bool connected;
    std::string error_message;
};

enum class VehicleDeletionResult
{
    deleted,
    not_found,
    currently_parked
};

class Database
{
public:
    explicit Database(const BackendConfig &config);

    DatabaseCheckResult checkConnection() const;
    pqxx::result executeRead(const std::string &query, const pqxx::params &parameters = {}) const;
    pqxx::result executeWrite(const std::string &query, const pqxx::params &parameters = {}) const;
    VehicleDeletionResult deleteVehicleAndHistory(
        std::int64_t vehicle_id,
        std::int64_t user_id) const;

private:
    pqxx::connection createConnection() const;

    BackendConfig config_;
};
