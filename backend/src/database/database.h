#pragma once

#include "../config/config.h"
#include "../api/session_responses.h"

#include <pqxx/pqxx>

#include <cstdint>
#include <optional>
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

enum class UserDeletionResult
{
    deleted,
    not_found,
    active_parking,
    future_reservations
};

enum class ReservationCreationStatus
{
    created,
    vehicle_not_found,
    vehicle_unavailable,
    slot_unavailable,
    invalid_time_range
};

enum class ReservationCheckInStatus
{
    checked_in,
    not_found,
    not_confirmed,
    too_early,
    expired,
    grace_period_exceeded,
    slot_unavailable,
    vehicle_unavailable
};

enum class ReservationCancellationStatus
{
    cancelled,
    not_found,
    not_cancellable
};

enum class SessionExitStatus
{
    completed,
    not_found,
    already_completed
};

struct ReservationCreationResult
{
    ReservationCreationStatus status;
    std::int64_t reservation_id = 0;
};

struct ReservationCheckInResult
{
    ReservationCheckInStatus status;
    std::int64_t session_id = 0;
};

struct SessionExitResult
{
    SessionExitStatus status;
    api::SessionFeeSummaryData fee_summary;
};

enum class VehicleParkingStatus
{
    parked,
    vehicle_not_found,
    slot_unavailable
};

struct VehicleParkingResult
{
    VehicleParkingStatus status;
    std::int64_t assignment_id = 0;
    std::int64_t slot_id = 0;
    std::int64_t area_id = 0;
    std::string slot_number;
    std::string area_name;
    std::string parked_at;
    std::string planned_end_time;
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
    UserDeletionResult deleteUserAndHistory(
        std::int64_t target_user_id) const;
    ReservationCreationResult createReservation(
        std::int64_t user_id,
        std::int64_t vehicle_id,
        std::int64_t slot_id,
        const std::string &start_time,
        const std::string &end_time) const;
    ReservationCheckInResult checkInReservation(
        std::int64_t user_id,
        std::int64_t reservation_id) const;
    ReservationCancellationStatus cancelReservation(
        std::int64_t user_id,
        std::int64_t reservation_id) const;
    void cancelOverdueReservations() const;
    SessionExitResult exitParkingSession(
        std::int64_t user_id,
        std::int64_t session_id,
        const std::string &payment_action = "paid_manually") const;
    std::optional<api::SessionFeeSummaryData> getSessionFeeSummary(
        std::int64_t user_id,
        std::int64_t session_id) const;
    VehicleParkingResult parkVehicle(
        std::int64_t user_id,
        std::int64_t vehicle_id,
        std::int64_t slot_id,
        int duration_minutes = 120) const;
    pqxx::connection createConnection() const;

private:
    BackendConfig config_;
};
