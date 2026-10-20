-- Vehicle Parking Management System
-- PostgreSQL database schema
-- 14 core tables
CREATE EXTENSION IF NOT EXISTS pgcrypto;
BEGIN;

-- ============================================================
-- 1. USERS
-- ============================================================

CREATE TABLE users (
    user_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255) NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    phone VARCHAR(20),
    role VARCHAR(20) NOT NULL DEFAULT 'user',
    is_active BOOLEAN NOT NULL DEFAULT TRUE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT users_role_check
        CHECK (role IN ('user', 'admin'))
);

CREATE TABLE auth_sessions (
    session_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id BIGINT NOT NULL,
    token_hash CHAR(64) NOT NULL UNIQUE,
    expires_at TIMESTAMPTZ NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT fk_auth_sessions_user
        FOREIGN KEY (user_id)
        REFERENCES users(user_id)
        ON DELETE CASCADE
);

CREATE INDEX auth_sessions_token_lookup
    ON auth_sessions(token_hash);

-- ============================================================
-- 2. AUTHENTICATION SESSIONS
-- ============================================================
-- auth_sessions is declared alongside users because it references users.

-- ============================================================
-- 3. VEHICLES
-- ============================================================

CREATE TABLE vehicles (
    vehicle_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id BIGINT NOT NULL,
    registration_number VARCHAR(20) NOT NULL UNIQUE,
    vehicle_type VARCHAR(30) NOT NULL,
    is_electric BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT fk_vehicles_user
        FOREIGN KEY (user_id)
        REFERENCES users(user_id)
        ON DELETE CASCADE,

    CONSTRAINT vehicles_type_check
        CHECK (vehicle_type IN ('car', 'bike', 'scooter', 'truck', 'bus', 'other'))
);

-- ============================================================
-- 4. PARKING AREAS
-- ============================================================

CREATE TABLE parking_areas (
    area_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name VARCHAR(150) NOT NULL,
    location TEXT NOT NULL,
    latitude NUMERIC(9,6),
    longitude NUMERIC(9,6),
    operating_hours TEXT,
    capacity INTEGER NOT NULL DEFAULT 0,
    status VARCHAR(20) NOT NULL DEFAULT 'active',
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT parking_areas_capacity_check
        CHECK (capacity >= 0),

    CONSTRAINT parking_areas_latitude_check
        CHECK (latitude IS NULL OR latitude BETWEEN -90 AND 90),

    CONSTRAINT parking_areas_longitude_check
        CHECK (longitude IS NULL OR longitude BETWEEN -180 AND 180),

    CONSTRAINT parking_areas_status_check
        CHECK (status IN ('active', 'inactive', 'closed'))
);

-- ============================================================
-- 5. PARKING SLOTS
-- ============================================================

CREATE TABLE parking_slots (
    slot_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    area_id BIGINT NOT NULL,
    slot_number VARCHAR(30) NOT NULL,
    vehicle_type VARCHAR(30) NOT NULL,
    slot_type VARCHAR(30) NOT NULL DEFAULT 'standard',
    is_ev BOOLEAN NOT NULL DEFAULT FALSE,
    status VARCHAR(20) NOT NULL DEFAULT 'available',
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT fk_parking_slots_area
        FOREIGN KEY (area_id)
        REFERENCES parking_areas(area_id)
        ON DELETE CASCADE,

    CONSTRAINT parking_slots_vehicle_type_check
        CHECK (vehicle_type IN ('car', 'bike', 'scooter', 'truck', 'bus', 'other')),

    CONSTRAINT parking_slots_status_check
        CHECK (status IN ('available', 'reserved', 'occupied', 'maintenance', 'unavailable')),

    CONSTRAINT parking_slots_unique_number
        UNIQUE (area_id, slot_number)
);

-- ============================================================
-- 6. ACTIVE PARKING ASSIGNMENTS
-- ============================================================

CREATE TABLE parking_assignments (
    assignment_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id BIGINT NOT NULL,
    vehicle_id BIGINT NOT NULL,
    slot_id BIGINT NOT NULL,
    parked_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    checkout_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT fk_parking_assignments_user
        FOREIGN KEY (user_id)
        REFERENCES users(user_id)
        ON DELETE RESTRICT,

    CONSTRAINT fk_parking_assignments_vehicle
        FOREIGN KEY (vehicle_id)
        REFERENCES vehicles(vehicle_id)
        ON DELETE RESTRICT,

    CONSTRAINT fk_parking_assignments_slot
        FOREIGN KEY (slot_id)
        REFERENCES parking_slots(slot_id)
        ON DELETE RESTRICT,

    CONSTRAINT parking_assignments_checkout_check
        CHECK (checkout_at IS NULL OR checkout_at >= parked_at)
);

CREATE UNIQUE INDEX parking_assignments_active_vehicle
    ON parking_assignments(vehicle_id)
    WHERE checkout_at IS NULL;

CREATE UNIQUE INDEX parking_assignments_active_slot
    ON parking_assignments(slot_id)
    WHERE checkout_at IS NULL;

CREATE INDEX parking_assignments_user_history
    ON parking_assignments(user_id, parked_at DESC);

-- ============================================================
-- 7. PARKING RATES
-- ============================================================

CREATE TABLE parking_rates (
    rate_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    area_id BIGINT NOT NULL,
    vehicle_type VARCHAR(30) NOT NULL,
    slot_type VARCHAR(30) NOT NULL DEFAULT 'standard',
    hourly_rate NUMERIC(10,2) NOT NULL,
    overstay_rate NUMERIC(10,2) NOT NULL DEFAULT 0,
    ev_charging_rate NUMERIC(10,2) NOT NULL DEFAULT 0,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT fk_parking_rates_area
        FOREIGN KEY (area_id)
        REFERENCES parking_areas(area_id)
        ON DELETE CASCADE,

    CONSTRAINT parking_rates_vehicle_type_check
        CHECK (vehicle_type IN ('car', 'bike', 'scooter', 'truck', 'bus', 'other')),

    CONSTRAINT parking_rates_hourly_rate_check
        CHECK (hourly_rate >= 0),

    CONSTRAINT parking_rates_overstay_rate_check
        CHECK (overstay_rate >= 0),

    CONSTRAINT parking_rates_ev_rate_check
        CHECK (ev_charging_rate >= 0),

    CONSTRAINT parking_rates_unique_rule
        UNIQUE (area_id, vehicle_type, slot_type)
);

-- ============================================================
-- 8. RESERVATIONS
-- ============================================================

CREATE TABLE reservations (
    reservation_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id BIGINT NOT NULL,
    vehicle_id BIGINT NOT NULL,
    slot_id BIGINT NOT NULL,
    start_time TIMESTAMPTZ NOT NULL,
    end_time TIMESTAMPTZ NOT NULL,
    status VARCHAR(20) NOT NULL DEFAULT 'pending',
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT fk_reservations_user
        FOREIGN KEY (user_id)
        REFERENCES users(user_id)
        ON DELETE RESTRICT,

    CONSTRAINT fk_reservations_vehicle
        FOREIGN KEY (vehicle_id)
        REFERENCES vehicles(vehicle_id)
        ON DELETE RESTRICT,

    CONSTRAINT fk_reservations_slot
        FOREIGN KEY (slot_id)
        REFERENCES parking_slots(slot_id)
        ON DELETE RESTRICT,

    CONSTRAINT reservations_time_check
        CHECK (end_time > start_time),

    CONSTRAINT reservations_status_check
        CHECK (status IN ('pending', 'confirmed', 'active', 'completed', 'expired', 'cancelled'))
);

-- ============================================================
-- 9. QR TOKENS
-- ============================================================

CREATE TABLE qr_tokens (
    qr_token_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    reservation_id BIGINT NOT NULL UNIQUE,
    token UUID NOT NULL UNIQUE DEFAULT gen_random_uuid(),
    expires_at TIMESTAMPTZ NOT NULL,
    is_used BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT fk_qr_tokens_reservation
        FOREIGN KEY (reservation_id)
        REFERENCES reservations(reservation_id)
        ON DELETE CASCADE
);

-- ============================================================
-- 10. PARKING SESSIONS
-- ============================================================

CREATE TABLE parking_sessions (
    session_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    reservation_id BIGINT NOT NULL UNIQUE,
    user_id BIGINT NOT NULL,
    vehicle_id BIGINT NOT NULL,
    slot_id BIGINT NOT NULL,
    entry_time TIMESTAMPTZ NOT NULL,
    exit_time TIMESTAMPTZ,
    duration_minutes INTEGER,
    status VARCHAR(20) NOT NULL DEFAULT 'active',
    overstay_duration_minutes INTEGER NOT NULL DEFAULT 0,
    calculated_fee NUMERIC(10,2) NOT NULL DEFAULT 0,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT fk_sessions_reservation
        FOREIGN KEY (reservation_id)
        REFERENCES reservations(reservation_id)
        ON DELETE RESTRICT,

    CONSTRAINT fk_sessions_user
        FOREIGN KEY (user_id)
        REFERENCES users(user_id)
        ON DELETE RESTRICT,

    CONSTRAINT fk_sessions_vehicle
        FOREIGN KEY (vehicle_id)
        REFERENCES vehicles(vehicle_id)
        ON DELETE RESTRICT,

    CONSTRAINT fk_sessions_slot
        FOREIGN KEY (slot_id)
        REFERENCES parking_slots(slot_id)
        ON DELETE RESTRICT,

    CONSTRAINT parking_sessions_exit_check
        CHECK (exit_time IS NULL OR exit_time >= entry_time),

    CONSTRAINT parking_sessions_duration_check
        CHECK (duration_minutes IS NULL OR duration_minutes >= 0),

    CONSTRAINT parking_sessions_overstay_check
        CHECK (overstay_duration_minutes >= 0),

    CONSTRAINT parking_sessions_fee_check
        CHECK (calculated_fee >= 0),

    CONSTRAINT parking_sessions_status_check
        CHECK (status IN ('active', 'completed', 'overstayed', 'cancelled'))
);

-- ============================================================
-- 11. MAINTENANCE
-- ============================================================

CREATE TABLE maintenance (
    maintenance_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    slot_id BIGINT NOT NULL,
    reason TEXT NOT NULL,
    start_time TIMESTAMPTZ NOT NULL,
    end_time TIMESTAMPTZ,
    status VARCHAR(20) NOT NULL DEFAULT 'scheduled',
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT fk_maintenance_slot
        FOREIGN KEY (slot_id)
        REFERENCES parking_slots(slot_id)
        ON DELETE CASCADE,

    CONSTRAINT maintenance_time_check
        CHECK (end_time IS NULL OR end_time >= start_time),

    CONSTRAINT maintenance_status_check
        CHECK (status IN ('scheduled', 'in_progress', 'completed', 'cancelled'))
);

-- ============================================================
-- 12. PAYMENTS
-- ============================================================

CREATE TABLE payments (
    payment_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    reservation_id BIGINT NOT NULL,
    session_id BIGINT,
    user_id BIGINT NOT NULL,
    amount NUMERIC(10,2) NOT NULL,
    payment_method VARCHAR(30) NOT NULL DEFAULT 'MOCK',
    status VARCHAR(20) NOT NULL DEFAULT 'pending',
    transaction_reference VARCHAR(100) UNIQUE,
    paid_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT fk_payments_reservation
        FOREIGN KEY (reservation_id)
        REFERENCES reservations(reservation_id)
        ON DELETE RESTRICT,

    CONSTRAINT fk_payments_session
        FOREIGN KEY (session_id)
        REFERENCES parking_sessions(session_id)
        ON DELETE RESTRICT,

    CONSTRAINT fk_payments_user
        FOREIGN KEY (user_id)
        REFERENCES users(user_id)
        ON DELETE RESTRICT,

    CONSTRAINT payments_amount_check
        CHECK (amount >= 0),

    CONSTRAINT payments_method_check
        CHECK (payment_method IN ('MOCK', 'UPI', 'CARD', 'CASH', 'OTHER')),

    CONSTRAINT payments_status_check
        CHECK (status IN ('pending', 'successful', 'failed', 'refunded')),

    CONSTRAINT payments_paid_at_check
        CHECK (
            (status = 'successful' AND paid_at IS NOT NULL)
            OR
            (status <> 'successful')
        )
);

-- ============================================================
-- 13. RECEIPTS
-- ============================================================

CREATE TABLE receipts (
    receipt_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    payment_id BIGINT NOT NULL UNIQUE,
    reservation_id BIGINT NOT NULL,
    session_id BIGINT NOT NULL,
    receipt_number VARCHAR(50) NOT NULL UNIQUE,
    base_fee NUMERIC(10,2) NOT NULL DEFAULT 0,
    overstay_fee NUMERIC(10,2) NOT NULL DEFAULT 0,
    additional_charges NUMERIC(10,2) NOT NULL DEFAULT 0,
    total_amount NUMERIC(10,2) NOT NULL,
    generated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT fk_receipts_payment
        FOREIGN KEY (payment_id)
        REFERENCES payments(payment_id)
        ON DELETE RESTRICT,

    CONSTRAINT fk_receipts_reservation
        FOREIGN KEY (reservation_id)
        REFERENCES reservations(reservation_id)
        ON DELETE RESTRICT,

    CONSTRAINT fk_receipts_session
        FOREIGN KEY (session_id)
        REFERENCES parking_sessions(session_id)
        ON DELETE RESTRICT,

    CONSTRAINT receipts_base_fee_check
        CHECK (base_fee >= 0),

    CONSTRAINT receipts_overstay_fee_check
        CHECK (overstay_fee >= 0),

    CONSTRAINT receipts_additional_charges_check
        CHECK (additional_charges >= 0),

    CONSTRAINT receipts_total_check
        CHECK (total_amount >= 0)
);

-- ============================================================
-- 14. NOTIFICATIONS
-- ============================================================

CREATE TABLE notifications (
    notification_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id BIGINT NOT NULL,
    title VARCHAR(150) NOT NULL,
    message TEXT NOT NULL,
    type VARCHAR(40) NOT NULL,
    is_read BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT fk_notifications_user
        FOREIGN KEY (user_id)
        REFERENCES users(user_id)
        ON DELETE CASCADE
);

-- ============================================================
-- INDEXES
-- ============================================================

CREATE INDEX idx_vehicles_user_id
    ON vehicles(user_id);

CREATE INDEX idx_parking_slots_area_id
    ON parking_slots(area_id);

CREATE INDEX idx_parking_slots_status
    ON parking_slots(status);

CREATE INDEX idx_parking_rates_area_id
    ON parking_rates(area_id);

CREATE INDEX idx_reservations_user_id
    ON reservations(user_id);

CREATE INDEX idx_reservations_vehicle_id
    ON reservations(vehicle_id);

CREATE INDEX idx_reservations_slot_id
    ON reservations(slot_id);

CREATE INDEX idx_reservations_status
    ON reservations(status);

CREATE INDEX idx_reservations_time
    ON reservations(start_time, end_time);

CREATE INDEX idx_qr_tokens_token
    ON qr_tokens(token);

CREATE INDEX idx_parking_sessions_user_id
    ON parking_sessions(user_id);

CREATE INDEX idx_parking_sessions_vehicle_id
    ON parking_sessions(vehicle_id);

CREATE INDEX idx_parking_sessions_slot_id
    ON parking_sessions(slot_id);

CREATE INDEX idx_parking_sessions_status
    ON parking_sessions(status);

CREATE INDEX idx_maintenance_slot_id
    ON maintenance(slot_id);

CREATE INDEX idx_maintenance_status
    ON maintenance(status);

CREATE INDEX idx_payments_user_id
    ON payments(user_id);

CREATE INDEX idx_payments_reservation_id
    ON payments(reservation_id);

CREATE INDEX idx_payments_status
    ON payments(status);

CREATE INDEX idx_receipts_reservation_id
    ON receipts(reservation_id);

CREATE INDEX idx_notifications_user_id
    ON notifications(user_id);

CREATE INDEX idx_notifications_unread
    ON notifications(user_id, is_read);

COMMIT;
