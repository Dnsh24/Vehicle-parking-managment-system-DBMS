#include "../src/api/errors.h"
#include "../src/api/parking_responses.h"
#include "../src/api/vehicle_responses.h"
#include "../src/api/session_responses.h"

#include <cassert>
#include <string>

int main()
{
    const crow::response response = api::errorResponse(
        503,
        "DATABASE_UNAVAILABLE",
        "Database is unavailable");

    assert(response.code == 503);
    assert(response.body.find("\"code\":\"DATABASE_UNAVAILABLE\"") != std::string::npos);
    assert(response.body.find("\"message\":\"Database is unavailable\"") != std::string::npos);

    const std::string area = api::areaJson(
        1,
        "Mall Central — Ground Floor",
        "Mall Central",
        "07:00 - 22:00",
        32,
        "active",
        12).dump();
    assert(area.find("\"area_id\":1") != std::string::npos);
    assert(area.find("\"capacity\":32") != std::string::npos);
    assert(area.find("\"available_slots\":12") != std::string::npos);

    const std::string slot = api::slotJson(
        10,
        1,
        "G-A01",
        "car",
        "standard",
        false,
        "available").dump();
    assert(slot.find("\"slot_number\":\"G-A01\"") != std::string::npos);
    assert(slot.find("\"status\":\"available\"") != std::string::npos);

    const std::string vehicle = api::vehicleJson(4, "ABC123", "car", true, false).dump();
    assert(vehicle.find("\"vehicle_id\":4") != std::string::npos);
    assert(vehicle.find("\"registration_number\":\"ABC123\"") != std::string::npos);
    assert(vehicle.find("\"vehicle_type\":\"car\"") != std::string::npos);
    assert(vehicle.find("\"is_electric\":true") != std::string::npos);
    assert(vehicle.find("\"needs_accessible_parking\":false") != std::string::npos);

    api::SessionFeeSummaryData summary;
    summary.session_id = 12;
    summary.reservation_id = 8;
    summary.vehicle_id = 4;
    summary.registration_number = "ABC123";
    summary.vehicle_type = "car";
    summary.slot_id = 10;
    summary.slot_number = "G-A01";
    summary.area_name = "Mall Central — Ground Floor";
    summary.location = "Mall Central";
    summary.entry_time = "2026-10-23T10:00:00Z";
    summary.exit_time = "2026-10-23T11:45:00Z";
    summary.reserved_end_time = "2026-10-23T11:30:00Z";
    summary.duration_minutes = 105;
    summary.billed_hours = 2;
    summary.overstay_duration_minutes = 15;
    summary.billed_overstay_hours = 1;
    summary.hourly_rate = 40.0;
    summary.overstay_rate = 60.0;
    summary.base_fee = 80.0;
    summary.overstay_fee = 60.0;
    summary.total_fee = 140.0;
    summary.payment_status = "paid_manually";
    summary.session_status = "overstayed";

    const std::string summary_str = api::sessionFeeSummaryJson(summary).dump();
    assert(summary_str.find("\"session_id\":12") != std::string::npos);
    assert(summary_str.find("\"duration_minutes\":105") != std::string::npos);
    assert(summary_str.find("\"billed_hours\":2") != std::string::npos);
    assert(summary_str.find("\"overstay_duration_minutes\":15") != std::string::npos);
    assert(summary_str.find("\"billed_overstay_hours\":1") != std::string::npos);
    assert(summary_str.find("\"base_fee\":80") != std::string::npos);
    assert(summary_str.find("\"overstay_fee\":60") != std::string::npos);
    assert(summary_str.find("\"total_fee\":140") != std::string::npos);
    assert(summary_str.find("\"payment_status\":\"paid_manually\"") != std::string::npos);
    assert(summary_str.find("\"status\":\"overstayed\"") != std::string::npos);
    assert(summary_str.find("\"grace_period_minutes\":7") != std::string::npos);
    assert(summary_str.find("\"overtime_rate_per_minute\":1") != std::string::npos);

    return 0;
}
