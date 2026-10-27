#pragma once

#include <algorithm>
#include <cstdint>

namespace api
{

struct FeeCalculationResult
{
    int duration_minutes = 0;              // Actual duration from entry to exit in minutes
    int booked_hours = 0;                  // Booked hours charged (rounded up to whole hours)
    int overstay_duration_minutes = 0;     // Total overtime elapsed past planned/reserved end time
    int grace_period_minutes = 7;          // Free grace period in minutes
    int billed_overstay_minutes = 0;       // Billable overtime minutes (past 7-minute grace)
    double hourly_rate = 0.0;              // Configured hourly rate for the slot
    double overtime_rate_per_minute = 1.0; // ₹1 per started overtime minute
    double base_fee = 0.0;                 // booked_hours * hourly_rate
    double overstay_fee = 0.0;             // billed_overstay_minutes * overtime_rate_per_minute
    double total_fee = 0.0;                // base_fee + overstay_fee
};

/**
 * Shared fee calculation helper for parking sessions and immediate parking assignments.
 * 
 * Rules:
 * - Booked slot time is rounded up to whole hours and charged at the slot's hourly rate.
 * - The full booked duration is charged even if the user checks out early.
 * - After the booked/planned end time, a 7-minute free grace period is provided.
 * - Overtime past the grace period is charged at ₹1 per started minute, independent of slot rate.
 * - Any partial minute after the grace period counts as one started minute.
 */
inline FeeCalculationResult calculateParkingFee(
    std::int64_t start_time_epoch,
    std::int64_t booked_end_time_epoch,
    std::int64_t exit_time_epoch,
    double hourly_rate,
    double overtime_rate_per_min = 1.0,
    int grace_mins = 7)
{
    FeeCalculationResult result;
    result.hourly_rate = hourly_rate;
    result.overtime_rate_per_minute = overtime_rate_per_min;
    result.grace_period_minutes = grace_mins;

    // Actual parking duration from start to exit
    const std::int64_t actual_duration_sec = std::max<std::int64_t>(0, exit_time_epoch - start_time_epoch);
    result.duration_minutes = static_cast<int>(std::max<std::int64_t>(1, (actual_duration_sec + 59) / 60));

    // Booked duration rounded up to whole hours (minimum 1 hour)
    const std::int64_t booked_duration_sec = std::max<std::int64_t>(0, booked_end_time_epoch - start_time_epoch);
    const int booked_duration_mins = static_cast<int>((booked_duration_sec + 59) / 60);
    result.booked_hours = std::max(1, (booked_duration_mins + 59) / 60);
    result.base_fee = result.booked_hours * hourly_rate;

    // Overtime past booked end time
    if (exit_time_epoch > booked_end_time_epoch)
    {
        const std::int64_t overtime_sec = exit_time_epoch - booked_end_time_epoch;
        result.overstay_duration_minutes = static_cast<int>((overtime_sec + 59) / 60);

        const std::int64_t grace_sec = static_cast<std::int64_t>(grace_mins) * 60;
        if (overtime_sec > grace_sec)
        {
            // Billed minutes for time past the 7-minute grace period
            // A partial minute after the grace period counts as one started minute
            const std::int64_t billable_sec = overtime_sec - grace_sec;
            result.billed_overstay_minutes = static_cast<int>((billable_sec + 59) / 60);
            result.overstay_fee = result.billed_overstay_minutes * overtime_rate_per_min;
        }
    }

    result.total_fee = result.base_fee + result.overstay_fee;
    return result;
}

} // namespace api

