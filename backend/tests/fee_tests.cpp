#include "api/fee_calculator.h"
#include "api/session_responses.h"
#include <cassert>
#include <cmath>
#include <iostream>

void testExactBookedHourRounding()
{
    // Start: 0s, Planned End: 3600s (1 hr), Exit: 3600s, Hourly Rate: 50.0
    const auto res = api::calculateParkingFee(0, 3600, 3600, 50.0);
    assert(res.booked_hours == 1);
    assert(res.base_fee == 50.0);
    assert(res.overstay_duration_minutes == 0);
    assert(res.billed_overstay_minutes == 0);
    assert(res.overstay_fee == 0.0);
    assert(res.total_fee == 50.0);
    std::cout << "testExactBookedHourRounding passed\n";
}

void testEarlyCheckoutFullBookedDuration()
{
    // Planned 2 hours (7200s), checkout early after 30 minutes (1800s)
    // Full booked duration (2 hours) must be charged even if user leaves early
    const auto res = api::calculateParkingFee(0, 7200, 1800, 40.0);
    assert(res.booked_hours == 2);
    assert(res.duration_minutes == 30);
    assert(res.base_fee == 80.0);
    assert(res.overstay_duration_minutes == 0);
    assert(res.billed_overstay_minutes == 0);
    assert(res.overstay_fee == 0.0);
    assert(res.total_fee == 80.0);
    std::cout << "testEarlyCheckoutFullBookedDuration passed\n";
}

void testWithinSevenMinuteGracePeriod()
{
    // Planned: 1 hour (3600s). Checkout at 3600 + 420s (exactly 7 minutes later)
    // Within 7-minute free grace period -> 0 billable overtime minutes, ₹0 overtime fee
    const auto res = api::calculateParkingFee(0, 3600, 3600 + 420, 60.0);
    assert(res.booked_hours == 1);
    assert(res.overstay_duration_minutes == 7);
    assert(res.billed_overstay_minutes == 0);
    assert(res.overstay_fee == 0.0);
    assert(res.total_fee == 60.0);
    std::cout << "testWithinSevenMinuteGracePeriod passed\n";
}

void testPartialMinuteAfterGraceCountsAsStartedMinute()
{
    // Planned: 3600s. Checkout at 3600 + 421s (7 minutes and 1 second later)
    // 1 second after grace period -> 1 started overtime minute = ₹1
    const auto res = api::calculateParkingFee(0, 3600, 3600 + 421, 50.0);
    assert(res.booked_hours == 1);
    assert(res.overstay_duration_minutes == 8);
    assert(res.billed_overstay_minutes == 1);
    assert(res.overstay_fee == 1.0);
    assert(res.total_fee == 51.0);
    std::cout << "testPartialMinuteAfterGraceCountsAsStartedMinute passed\n";
}

void testThirtyMinutesOvertimeExample()
{
    // Prompt specification: "30 minutes after the booked end means 7 free minutes plus 23 billable minutes = ₹23 overtime"
    // Planned: 3600s (1 hr). Exit at 3600 + 1800s (30 minutes overstay)
    // Hourly rate: 100.0 (overtime rate is ₹1/min independent of hourly rate)
    const auto res = api::calculateParkingFee(0, 3600, 3600 + 1800, 100.0);
    assert(res.booked_hours == 1);
    assert(res.overstay_duration_minutes == 30);
    assert(res.grace_period_minutes == 7);
    assert(res.billed_overstay_minutes == 23);
    assert(res.overstay_fee == 23.0);
    assert(res.base_fee == 100.0);
    assert(res.total_fee == 123.0);
    std::cout << "testThirtyMinutesOvertimeExample passed\n";
}

void testOvertimeIndependentOfHourlyRate()
{
    // Slot hourly rate is ₹250/hr. 45 minutes overtime.
    // 45 - 7 = 38 billable minutes -> ₹38 overtime, NOT based on ₹250/hr
    const auto res = api::calculateParkingFee(0, 7200, 7200 + 2700, 250.0);
    assert(res.booked_hours == 2);
    assert(res.base_fee == 500.0);
    assert(res.overstay_duration_minutes == 45);
    assert(res.billed_overstay_minutes == 38);
    assert(res.overstay_fee == 38.0);
    assert(res.total_fee == 538.0);
    std::cout << "testOvertimeIndependentOfHourlyRate passed\n";
}

void testDurationLessThanOneHourRoundedToOneHourCost()
{
    // User planned 20 minutes (1200s). Rate = 40.0. Exits on time at 1200s.
    // Must round up to 1 hour cost: 1 * 40.0 = 40.0
    const auto res = api::calculateParkingFee(0, 1200, 1200, 40.0);
    assert(res.booked_hours == 1);
    assert(res.duration_minutes == 20);
    assert(res.base_fee == 40.0);
    assert(res.overstay_duration_minutes == 0);
    assert(res.billed_overstay_minutes == 0);
    assert(res.overstay_fee == 0.0);
    assert(res.total_fee == 40.0);
    std::cout << "testDurationLessThanOneHourRoundedToOneHourCost passed\n";
}

void testDurationLessThanOneHourWithOverstayAndGrace()
{
    // User planned 30 minutes (1800s). Rate = 40.0.
    // Exits at 45 minutes (2700s) -> 15 minutes overstay.
    // 7-min free grace period -> 8 billable minutes @ ₹1/min = ₹8.
    // Base fee is rounded up to 1 hr cost = ₹40. Total = ₹48.
    const auto res = api::calculateParkingFee(0, 1800, 2700, 40.0);
    assert(res.booked_hours == 1);
    assert(res.duration_minutes == 45);
    assert(res.base_fee == 40.0);
    assert(res.overstay_duration_minutes == 15);
    assert(res.grace_period_minutes == 7);
    assert(res.billed_overstay_minutes == 8);
    assert(res.overstay_fee == 8.0);
    assert(res.total_fee == 48.0);
    std::cout << "testDurationLessThanOneHourWithOverstayAndGrace passed\n";
}

void testInBetweenDurationRounding()
{
    // User planned 90 minutes (1.5 hrs = 5400s). Rate = 30.0.
    // 90 minutes rounds up to 2 whole hours = 2 * 30.0 = 60.0.
    // Exits at 95 minutes (within 7-min grace period) -> ₹0 overtime.
    const auto res = api::calculateParkingFee(0, 5400, 5700, 30.0);
    assert(res.booked_hours == 2);
    assert(res.duration_minutes == 95);
    assert(res.base_fee == 60.0);
    assert(res.overstay_duration_minutes == 5);
    assert(res.billed_overstay_minutes == 0);
    assert(res.overstay_fee == 0.0);
    assert(res.total_fee == 60.0);
    std::cout << "testInBetweenDurationRounding passed\n";
}

void testSessionFeeSummaryJsonSerialization()
{
    api::SessionFeeSummaryData data;
    data.session_id = 42;
    data.registration_number = "KA01AB1234";
    data.vehicle_type = "car";
    data.hourly_rate = 50.0;
    data.base_fee = 100.0;
    data.duration_minutes = 150;
    data.booked_hours = 2;
    data.billed_hours = 2;
    data.overstay_duration_minutes = 30;
    data.grace_period_minutes = 7;
    data.billed_overstay_minutes = 23;
    data.billed_overtime_minutes = 23;
    data.overtime_rate_per_minute = 1.0;
    data.overstay_fee = 23.0;
    data.total_fee = 123.0;
    data.planned_end_time = "2026-10-27T12:00:00Z";
    data.reserved_end_time = "2026-10-27T12:00:00Z";

    const auto json = api::sessionFeeSummaryJson(data);
    const std::string dumped = json.dump();
    const auto parsed = crow::json::load(dumped);

    assert(parsed.has("grace_period_minutes"));
    assert(parsed["grace_period_minutes"].i() == 7);
    assert(parsed.has("billed_overstay_minutes"));
    assert(parsed["billed_overstay_minutes"].i() == 23);
    assert(parsed.has("billed_overtime_minutes"));
    assert(parsed["billed_overtime_minutes"].i() == 23);
    assert(parsed.has("overtime_rate_per_minute"));
    assert(parsed["overtime_rate_per_minute"].d() == 1.0);
    assert(parsed.has("overstay_fee"));
    assert(parsed["overstay_fee"].d() == 23.0);
    assert(parsed.has("planned_end_time"));
    assert(parsed["planned_end_time"].s() == "2026-10-27T12:00:00Z");
    std::cout << "testSessionFeeSummaryJsonSerialization passed\n";
}

int main()
{
    testExactBookedHourRounding();
    testEarlyCheckoutFullBookedDuration();
    testWithinSevenMinuteGracePeriod();
    testPartialMinuteAfterGraceCountsAsStartedMinute();
    testThirtyMinutesOvertimeExample();
    testOvertimeIndependentOfHourlyRate();
    testDurationLessThanOneHourRoundedToOneHourCost();
    testDurationLessThanOneHourWithOverstayAndGrace();
    testInBetweenDurationRounding();
    testSessionFeeSummaryJsonSerialization();
    std::cout << "All fee calculation tests passed successfully!\n";
    return 0;
}
