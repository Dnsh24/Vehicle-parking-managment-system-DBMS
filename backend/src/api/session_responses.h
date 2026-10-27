#pragma once

#include "crow/json.h"
#include <string>
#include <cstdint>

namespace api
{

struct SessionFeeSummaryData
{
    std::int64_t session_id = 0;
    std::int64_t reservation_id = 0;
    std::int64_t vehicle_id = 0;
    std::string registration_number;
    std::string vehicle_type;
    std::int64_t slot_id = 0;
    std::string slot_number;
    std::string area_name;
    std::string location;
    std::string entry_time;
    std::string exit_time;
    std::string reserved_end_time;
    std::string planned_end_time;
    int duration_minutes = 0;
    int booked_hours = 0;
    int billed_hours = 0;
    int overstay_duration_minutes = 0;
    int billed_overstay_hours = 0;
    int grace_period_minutes = 7;
    int billed_overstay_minutes = 0;
    int billed_overtime_minutes = 0;
    double hourly_rate = 0.0;
    double overstay_rate = 0.0;
    double overtime_rate_per_minute = 1.0;
    double base_fee = 0.0;
    double overstay_fee = 0.0;
    double total_fee = 0.0;
    std::string payment_status;
    std::string session_status;
};

inline crow::json::wvalue sessionFeeSummaryJson(const SessionFeeSummaryData &data)
{
    crow::json::wvalue json;
    json["session_id"] = data.session_id;
    json["reservation_id"] = data.reservation_id;
    json["vehicle_id"] = data.vehicle_id;
    json["registration_number"] = data.registration_number;
    json["vehicle_type"] = data.vehicle_type;
    json["slot_id"] = data.slot_id;
    json["slot_number"] = data.slot_number;
    json["area_name"] = data.area_name;
    json["location"] = data.location;
    json["entry_time"] = data.entry_time;
    if (!data.exit_time.empty())
    {
        json["exit_time"] = data.exit_time;
    }
    json["reserved_end_time"] = data.reserved_end_time;
    json["planned_end_time"] = data.planned_end_time.empty() ? data.reserved_end_time : data.planned_end_time;
    json["duration_minutes"] = data.duration_minutes;
    json["booked_hours"] = data.booked_hours > 0 ? data.booked_hours : data.billed_hours;
    json["billed_hours"] = data.billed_hours;
    json["overstay_duration_minutes"] = data.overstay_duration_minutes;
    json["billed_overstay_hours"] = data.billed_overstay_hours;
    json["grace_period_minutes"] = data.grace_period_minutes;
    json["billed_overstay_minutes"] = data.billed_overstay_minutes;
    json["billed_overtime_minutes"] = data.billed_overtime_minutes;
    json["hourly_rate"] = data.hourly_rate;
    json["overstay_rate"] = data.overstay_rate;
    json["overtime_rate_per_minute"] = data.overtime_rate_per_minute;
    json["base_fee"] = data.base_fee;
    json["overstay_fee"] = data.overstay_fee;
    json["total_fee"] = data.total_fee;
    json["payment_status"] = data.payment_status;
    json["status"] = data.session_status;
    return json;
}

} // namespace api
