-- Vehicle Parking Management System
-- PostgreSQL database schema
-- 12 core tables
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
    needs_accessible_parking BOOLEAN NOT NULL DEFAULT FALSE,
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
    planned_end_time TIMESTAMPTZ,
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
        CHECK (checkout_at IS NULL OR checkout_at >= parked_at),

    CONSTRAINT parking_assignments_planned_end_check
        CHECK (planned_end_time IS NULL OR planned_end_time > parked_at)
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
-- 9. PARKING SESSIONS
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
-- 10. MAINTENANCE
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
-- 11. PAYMENTS
-- ============================================================

CREATE TABLE payments (
    payment_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    reservation_id BIGINT,
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
-- 12. NOTIFICATIONS
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

CREATE INDEX idx_notifications_user_id
    ON notifications(user_id);

CREATE INDEX idx_notifications_unread
    ON notifications(user_id, is_read);


-- ============================================================
-- VIEWS
-- ============================================================

CREATE OR REPLACE VIEW v_parking_area_revenue_report AS
SELECT 
    pa.area_id,
    pa.name AS area_name,
    pa.location,
    pa.capacity,
    COUNT(DISTINCT ps.slot_id) AS total_slots,
    COUNT(DISTINCT s.session_id) AS total_sessions,
    COALESCE(SUM(p.amount), 0.00) AS total_revenue,
    COALESCE(ROUND(AVG(s.duration_minutes), 2), 0.00) AS avg_duration_minutes,
    COALESCE(ROUND(AVG(p.amount), 2), 0.00) AS avg_revenue_per_session
FROM parking_areas pa
LEFT JOIN parking_slots ps ON ps.area_id = pa.area_id
LEFT JOIN parking_sessions s ON s.slot_id = ps.slot_id AND s.status IN ('completed', 'overstayed')
LEFT JOIN payments p ON p.session_id = s.session_id AND p.status = 'successful'
GROUP BY pa.area_id, pa.name, pa.location, pa.capacity;

CREATE OR REPLACE VIEW v_active_parking_status AS
SELECT
    ps.slot_id,
    pa.name AS area_name,
    ps.slot_number,
    ps.vehicle_type,
    ps.slot_type,
    ps.is_ev,
    ps.status AS slot_status,
    u.name AS parked_user_name,
    v.registration_number,
    s.entry_time,
    s.status AS session_status
FROM parking_slots ps
JOIN parking_areas pa ON pa.area_id = ps.area_id
LEFT JOIN parking_sessions s ON s.slot_id = ps.slot_id AND s.exit_time IS NULL AND s.status IN ('active', 'overstayed')
LEFT JOIN vehicles v ON v.vehicle_id = s.vehicle_id
LEFT JOIN users u ON u.user_id = s.user_id;

-- ============================================================
-- FUNCTIONS AND STORED PROCEDURES
-- ============================================================

CREATE OR REPLACE FUNCTION fn_get_available_slots_count(p_area_id BIGINT)
RETURNS INTEGER AS $$
DECLARE
    v_available_count INTEGER;
BEGIN
    SELECT COUNT(*)
    INTO v_available_count
    FROM parking_slots
    WHERE area_id = p_area_id
      AND status = 'available';
    RETURN v_available_count;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION fn_calculate_estimated_fee(
    p_area_id BIGINT,
    p_vehicle_type VARCHAR,
    p_slot_type VARCHAR,
    p_hours NUMERIC
)
RETURNS NUMERIC AS $$
DECLARE
    v_rate NUMERIC(10,2);
    v_total NUMERIC(10,2);
BEGIN
    SELECT hourly_rate
    INTO v_rate
    FROM parking_rates
    WHERE area_id = p_area_id
      AND vehicle_type = p_vehicle_type
      AND slot_type = p_slot_type;

    IF v_rate IS NULL THEN
        v_rate := 30.00;
    END IF;

    v_total := CEIL(p_hours) * v_rate;
    RETURN v_total;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE PROCEDURE sp_record_maintenance(
    p_slot_id BIGINT,
    p_reason TEXT,
    p_start_time TIMESTAMPTZ,
    p_end_time TIMESTAMPTZ
)
LANGUAGE plpgsql
AS $$
BEGIN
    INSERT INTO maintenance (slot_id, reason, start_time, end_time, status)
    VALUES (p_slot_id, p_reason, p_start_time, p_end_time, 'scheduled');
    
    UPDATE parking_slots
    SET status = 'maintenance'
    WHERE slot_id = p_slot_id;
END;
$$;

-- ============================================================
-- TRIGGERS
-- ============================================================

CREATE OR REPLACE FUNCTION trg_fn_sync_slot_maintenance()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.status IN ('scheduled', 'in_progress') THEN
        UPDATE parking_slots
        SET status = 'maintenance'
        WHERE slot_id = NEW.slot_id;
    ELSIF NEW.status IN ('completed', 'cancelled') THEN
        UPDATE parking_slots
        SET status = 'available'
        WHERE slot_id = NEW.slot_id
          AND status = 'maintenance';
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_maintenance_slot_status ON maintenance;
CREATE TRIGGER trg_maintenance_slot_status
AFTER INSERT OR UPDATE OF status ON maintenance
FOR EACH ROW
EXECUTE FUNCTION trg_fn_sync_slot_maintenance();

COMMIT;
