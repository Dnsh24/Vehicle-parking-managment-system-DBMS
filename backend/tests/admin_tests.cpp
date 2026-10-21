#include "../src/api/admin_responses.h"

#include <cassert>
#include <string>

int main()
{
    const std::string user = api::adminUserJson(
        1,
        "admin",
        "Admin User",
        "admin@example.com",
        "1234567890",
        "admin",
        true,
        "2026-01-01T00:00:00Z")
        .dump();
    assert(user.find("\"user_id\":1") != std::string::npos);
    assert(user.find("\"username\":\"admin\"") != std::string::npos);
    assert(user.find("\"name\":\"Admin User\"") != std::string::npos);
    assert(user.find("\"email\":\"admin@example.com\"") != std::string::npos);
    assert(user.find("\"phone\":\"1234567890\"") != std::string::npos);
    assert(user.find("\"role\":\"admin\"") != std::string::npos);
    assert(user.find("\"is_active\":true") != std::string::npos);
    assert(user.find("\"created_at\":\"2026-01-01T00:00:00Z\"") != std::string::npos);

    const std::string inactive_user = api::adminUserJson(
        2,
        "regular",
        "Regular User",
        "user@example.com",
        "",
        "user",
        false,
        "2026-02-02T00:00:00Z")
        .dump();
    assert(inactive_user.find("\"role\":\"user\"") != std::string::npos);
    assert(inactive_user.find("\"is_active\":false") != std::string::npos);

    const std::string area = api::adminAreaJson(
        3,
        "Mall Central - Ground Floor",
        "Mall Central",
        "07:00 - 22:00",
        32,
        "active",
        "2026-01-01T00:00:00Z",
        28)
        .dump();
    assert(area.find("\"area_id\":3") != std::string::npos);
    assert(area.find("\"name\":\"Mall Central - Ground Floor\"") != std::string::npos);
    assert(area.find("\"location\":\"Mall Central\"") != std::string::npos);
    assert(area.find("\"operating_hours\":\"07:00 - 22:00\"") != std::string::npos);
    assert(area.find("\"capacity\":32") != std::string::npos);
    assert(area.find("\"status\":\"active\"") != std::string::npos);
    assert(area.find("\"slot_count\":28") != std::string::npos);
    assert(area.find("\"created_at\":\"2026-01-01T00:00:00Z\"") != std::string::npos);

    const std::string slot = api::adminSlotJson(
        10,
        3,
        "Mall Central - Ground Floor",
        "G-A01",
        "car",
        "standard",
        false,
        "available",
        "2026-01-01T00:00:00Z")
        .dump();
    assert(slot.find("\"slot_id\":10") != std::string::npos);
    assert(slot.find("\"area_id\":3") != std::string::npos);
    assert(slot.find("\"area_name\":\"Mall Central - Ground Floor\"") != std::string::npos);
    assert(slot.find("\"slot_number\":\"G-A01\"") != std::string::npos);
    assert(slot.find("\"vehicle_type\":\"car\"") != std::string::npos);
    assert(slot.find("\"slot_type\":\"standard\"") != std::string::npos);
    assert(slot.find("\"is_ev\":false") != std::string::npos);
    assert(slot.find("\"status\":\"available\"") != std::string::npos);
    assert(slot.find("\"created_at\":\"2026-01-01T00:00:00Z\"") != std::string::npos);

    const std::string ev_slot = api::adminSlotJson(
        11,
        3,
        "Mall Central - Ground Floor",
        "G-A02",
        "car",
        "ev",
        true,
        "maintenance",
        "2026-03-03T00:00:00Z")
        .dump();
    assert(ev_slot.find("\"is_ev\":true") != std::string::npos);
    assert(ev_slot.find("\"slot_type\":\"ev\"") != std::string::npos);
    assert(ev_slot.find("\"status\":\"maintenance\"") != std::string::npos);

    const std::string rate = api::adminRateJson(
        7,
        3,
        "Mall Central - Ground Floor",
        "car",
        "standard",
        50.00,
        75.00,
        10.00,
        "2026-01-01T00:00:00Z")
        .dump();
    assert(rate.find("\"rate_id\":7") != std::string::npos);
    assert(rate.find("\"area_id\":3") != std::string::npos);
    assert(rate.find("\"area_name\":\"Mall Central - Ground Floor\"") != std::string::npos);
    assert(rate.find("\"vehicle_type\":\"car\"") != std::string::npos);
    assert(rate.find("\"slot_type\":\"standard\"") != std::string::npos);
    assert(rate.find("\"hourly_rate\":50") != std::string::npos);
    assert(rate.find("\"overstay_rate\":75") != std::string::npos);
    assert(rate.find("\"ev_charging_rate\":10") != std::string::npos);
    assert(rate.find("\"created_at\":\"2026-01-01T00:00:00Z\"") != std::string::npos);

    const std::string maintenance = api::adminMaintenanceJson(
        4,
        10,
        "G-A01",
        3,
        "Mall Central - Ground Floor",
        "Lighting repair",
        "2026-11-01T08:00:00Z",
        "2026-11-01T12:00:00Z",
        "in_progress",
        "2026-10-31T00:00:00Z")
        .dump();
    assert(maintenance.find("\"maintenance_id\":4") != std::string::npos);
    assert(maintenance.find("\"slot_id\":10") != std::string::npos);
    assert(maintenance.find("\"slot_number\":\"G-A01\"") != std::string::npos);
    assert(maintenance.find("\"area_id\":3") != std::string::npos);
    assert(maintenance.find("\"area_name\":\"Mall Central - Ground Floor\"") != std::string::npos);
    assert(maintenance.find("\"reason\":\"Lighting repair\"") != std::string::npos);
    assert(maintenance.find("\"start_time\":\"2026-11-01T08:00:00Z\"") != std::string::npos);
    assert(maintenance.find("\"end_time\":\"2026-11-01T12:00:00Z\"") != std::string::npos);
    assert(maintenance.find("\"status\":\"in_progress\"") != std::string::npos);
    assert(maintenance.find("\"created_at\":\"2026-10-31T00:00:00Z\"") != std::string::npos);

    return 0;
}