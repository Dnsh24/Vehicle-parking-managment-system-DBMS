BEGIN;

CREATE TABLE IF NOT EXISTS parking_assignments (
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

CREATE UNIQUE INDEX IF NOT EXISTS parking_assignments_active_vehicle
    ON parking_assignments(vehicle_id)
    WHERE checkout_at IS NULL;

CREATE UNIQUE INDEX IF NOT EXISTS parking_assignments_active_slot
    ON parking_assignments(slot_id)
    WHERE checkout_at IS NULL;

CREATE INDEX IF NOT EXISTS parking_assignments_user_history
    ON parking_assignments(user_id, parked_at DESC);

UPDATE parking_slots ps
SET status = 'available'
WHERE ps.status IN ('occupied', 'reserved')
  AND NOT EXISTS (
      SELECT 1
      FROM parking_sessions s
      WHERE s.slot_id = ps.slot_id
        AND s.exit_time IS NULL
        AND s.status IN ('active', 'overstayed')
  )
  AND NOT EXISTS (
      SELECT 1
      FROM reservations r
      WHERE r.slot_id = ps.slot_id
        AND r.status IN ('pending', 'confirmed', 'active')
        AND r.start_time <= CURRENT_TIMESTAMP
        AND r.end_time > CURRENT_TIMESTAMP
  )
  AND NOT EXISTS (
      SELECT 1
      FROM parking_assignments a
      WHERE a.slot_id = ps.slot_id
        AND a.checkout_at IS NULL
  );

DELETE FROM vehicles v
WHERE NOT EXISTS (
    SELECT 1 FROM reservations r WHERE r.vehicle_id = v.vehicle_id
)
AND NOT EXISTS (
    SELECT 1 FROM parking_sessions s WHERE s.vehicle_id = v.vehicle_id
)
AND NOT EXISTS (
    SELECT 1 FROM parking_assignments a WHERE a.vehicle_id = v.vehicle_id
);

COMMIT;
