-- Phase 4 development seed data
-- Administrator account (dnshneela@gmail.com / @24Dinesh), baseline parking rates, and slot maintenance records.
-- Safe to re-run.
BEGIN;

-- 1. Sole administrator account: dnshneela@gmail.com / @24Dinesh
INSERT INTO users (
    name,
    email,
    username,
    password_hash,
    phone,
    role,
    is_active
)
SELECT
    'Dinesh Neela',
    'dnshneela@gmail.com',
    'dnshneela',
    'pbkdf2_sha256$210000$99b8e38a15d9b9ef73dff7093b6d48a3$b967d9b28025edb7f722e7a69fce6075cab8460411bb5c544f37f6116ee862f0',
    '+919876543210',
    'admin',
    TRUE
WHERE NOT EXISTS (
    SELECT 1 FROM users WHERE email = 'dnshneela@gmail.com'
);

-- Ensure dnshneela has admin role and is active with updated password
UPDATE users
SET password_hash = 'pbkdf2_sha256$210000$99b8e38a15d9b9ef73dff7093b6d48a3$b967d9b28025edb7f722e7a69fce6075cab8460411bb5c544f37f6116ee862f0',
    role = 'admin',
    is_active = TRUE
WHERE email = 'dnshneela@gmail.com';

-- Ensure ONLY dnshneela@gmail.com has admin access
UPDATE users
SET role = 'user'
WHERE email != 'dnshneela@gmail.com';

-- 2. Baseline parking rates for all active parking areas
INSERT INTO parking_rates (
    area_id,
    vehicle_type,
    slot_type,
    hourly_rate,
    overstay_rate,
    ev_charging_rate
)
SELECT
    pa.area_id,
    rates.vehicle_type,
    rates.slot_type,
    rates.hourly_rate,
    rates.overstay_rate,
    rates.ev_charging_rate
FROM parking_areas pa
CROSS JOIN (
    VALUES
        ('car', 'standard', 40.00, 60.00, 0.00),
        ('car', 'ev', 50.00, 75.00, 15.00),
        ('bike', 'two-wheeler', 20.00, 30.00, 0.00),
        ('scooter', 'two-wheeler', 15.00, 25.00, 0.00),
        ('truck', 'truck', 80.00, 120.00, 0.00)
) AS rates(vehicle_type, slot_type, hourly_rate, overstay_rate, ev_charging_rate)
WHERE NOT EXISTS (
    SELECT 1 FROM parking_rates pr
    WHERE pr.area_id = pa.area_id
      AND pr.vehicle_type = rates.vehicle_type
      AND pr.slot_type = rates.slot_type
);

-- 3. Initial maintenance records for slots currently marked as 'maintenance'
INSERT INTO maintenance (
    slot_id,
    reason,
    start_time,
    end_time,
    status
)
SELECT
    ps.slot_id,
    'Scheduled sensor calibration and bay resurfacing',
    CURRENT_TIMESTAMP - INTERVAL '2 hours',
    CURRENT_TIMESTAMP + INTERVAL '6 hours',
    'in_progress'
FROM parking_slots ps
WHERE ps.status = 'maintenance'
  AND NOT EXISTS (
      SELECT 1 FROM maintenance m WHERE m.slot_id = ps.slot_id AND m.status IN ('scheduled', 'in_progress')
  );

COMMIT;
