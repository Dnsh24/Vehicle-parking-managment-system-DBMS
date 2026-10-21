#pragma once

#include "crow.h"

#include <cstdint>
#include <string>

namespace api
{
inline crow::json::wvalue adminUserJson(
    std::int64_t user_id,
    const std::string &username,
    const std::string &name,
    const std::string &email,
    const std::string &phone,
    const std::string &role,
    bool is_active,
    const std::string &created_at)
{
    crow::json::wvalue user;
    user["user_id"] = user_id;
    user["username"] = username;
    user["name"] = name;
    user["email"] = email;
    user["phone"] = phone;
    user["role"] = role;
    user["is_active"] = is_active;
    user["created_at"] = created_at;
    return user;
}

inline crow::json::wvalue adminAreaJson(
    std::int64_t area_id,
    const std::string &name,
    const std::string &location,
    const std::string &operating_hours,
    int capacity,
    const std::string &status,
    const std::string &created_at,
    std::int64_t slot_count)
{
    crow::json::wvalue area;
    area["area_id"] = area_id;
    area["name"] = name;
    area["location"] = location;
    area["operating_hours"] = operating_hours;
    area["capacity"] = capacity;
    area["status"] = status;
    area["created_at"] = created_at;
    area["slot_count"] = slot_count;
    return area;
}

inline crow::json::wvalue adminSlotJson(
    std::int64_t slot_id,
    std::int64_t area_id,
    const std::string &area_name,
    const std::string &slot_number,
    const std::string &vehicle_type,
    const std::string &slot_type,
    bool is_ev,
    const std::string &status,
    const std::string &created_at)
{
    crow::json::wvalue slot;
    slot["slot_id"] = slot_id;
    slot["area_id"] = area_id;
    slot["area_name"] = area_name;
    slot["slot_number"] = slot_number;
    slot["vehicle_type"] = vehicle_type;
    slot["slot_type"] = slot_type;
    slot["is_ev"] = is_ev;
    slot["status"] = status;
    slot["created_at"] = created_at;
    return slot;
}

inline crow::json::wvalue adminRateJson(
    std::int64_t rate_id,
    std::int64_t area_id,
    const std::string &area_name,
    const std::string &vehicle_type,
    const std::string &slot_type,
    double hourly_rate,
    double overstay_rate,
    double ev_charging_rate,
    const std::string &created_at)
{
    crow::json::wvalue rate;
    rate["rate_id"] = rate_id;
    rate["area_id"] = area_id;
    rate["area_name"] = area_name;
    rate["vehicle_type"] = vehicle_type;
    rate["slot_type"] = slot_type;
    rate["hourly_rate"] = hourly_rate;
    rate["overstay_rate"] = overstay_rate;
    rate["ev_charging_rate"] = ev_charging_rate;
    rate["created_at"] = created_at;
    return rate;
}

inline crow::json::wvalue adminMaintenanceJson(
    std::int64_t maintenance_id,
    std::int64_t slot_id,
    const std::string &slot_number,
    std::int64_t area_id,
    const std::string &area_name,
    const std::string &reason,
    const std::string &start_time,
    const std::string &end_time,
    const std::string &status,
    const std::string &created_at)
{
    crow::json::wvalue maintenance;
    maintenance["maintenance_id"] = maintenance_id;
    maintenance["slot_id"] = slot_id;
    maintenance["slot_number"] = slot_number;
    maintenance["area_id"] = area_id;
    maintenance["area_name"] = area_name;
    maintenance["reason"] = reason;
    maintenance["start_time"] = start_time;
    maintenance["end_time"] = end_time;
    maintenance["status"] = status;
    maintenance["created_at"] = created_at;
    return maintenance;
}
} // namespace api