#include "../src/validation/validation.h"

#include <cassert>

int main()
{
    assert(validation::requiredString("value", "name").valid);
    assert(!validation::requiredString("  ", "name").valid);

    assert(validation::positiveInteger("42", "area_id").valid);
    assert(!validation::positiveInteger("0", "area_id").valid);
    assert(!validation::positiveInteger("4x", "area_id").valid);

    assert(validation::email("user@example.com").valid);
    assert(!validation::email("not-an-email").valid);

    assert(validation::allowedValue("car", "vehicle_type", {"car", "bike"}).valid);
    assert(!validation::allowedValue("bus", "vehicle_type", {"car", "bike"}).valid);

    assert(validation::timestamp("2026-10-18T12:00:00Z", "start_time").valid);
    assert(validation::timestamp("2026-10-18T12:00:00.000Z", "start_time").valid);
    assert(validation::timestamp("2026-10-18T12:00:00+05:30", "start_time").valid);
    assert(validation::timestamp("2026-10-18T12:00:00.123+05:30", "start_time").valid);
    assert(!validation::timestamp("18-10-2026", "start_time").valid);
    assert(!validation::timestamp("2026-02-30T12:00:00Z", "start_time").valid);
    assert(!validation::timestamp("2026-10-18T25:00:00Z", "start_time").valid);
    assert(!validation::timestamp("2026-10-18T12:00:00+14:30", "start_time").valid);

    return 0;
}
