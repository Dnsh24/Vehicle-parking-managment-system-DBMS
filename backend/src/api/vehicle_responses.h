#pragma once

#include "crow.h"

#include <cstdint>
#include <string>

namespace api
{
inline crow::json::wvalue vehicleJson(
    std::int64_t vehicle_id,
    const std::string &registration_number,
    const std::string &vehicle_type,
    bool is_electric,
    bool needs_accessible_parking)
{
    crow::json::wvalue response;
    response["vehicle_id"] = vehicle_id;
    response["registration_number"] = registration_number;
    response["vehicle_type"] = vehicle_type;
    response["is_electric"] = is_electric;
    response["needs_accessible_parking"] = needs_accessible_parking;
    return response;
}
}
