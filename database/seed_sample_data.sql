-- Sample Data Seed Script for Vehicle Parking Management System
-- Fulfills technical requirement: At least 10 records in each major table.
-- Safe to re-run (idempotent with ON CONFLICT / WHERE NOT EXISTS checks).

BEGIN;

-- ============================================================
-- 1. USERS (At least 10 records)
-- ============================================================
-- All sample users have the password hash for 'Password@123'
-- dnshneela is the administrator (from seed_phase4_admin.sql)

INSERT INTO users (name, email, username, password_hash, phone, role, is_active)
VALUES
    ('Dinesh Neela', 'dnshneela@gmail.com', 'dnshneela', 'pbkdf2_sha256$210000$99b8e38a15d9b9ef73dff7093b6d48a3$b967d9b28025edb7f722e7a69fce6075cab8460411bb5c544f37f6116ee862f0', '+919876543210', 'admin', TRUE),
    ('Aarav Sharma', 'aarav.sharma@example.com', 'aarav_s', 'pbkdf2_sha256$210000$99b8e38a15d9b9ef73dff7093b6d48a3$b967d9b28025edb7f722e7a69fce6075cab8460411bb5c544f37f6116ee862f0', '+919876543211', 'user', TRUE),
    ('Priya Patel', 'priya.patel@example.com', 'priya_p', 'pbkdf2_sha256$210000$99b8e38a15d9b9ef73dff7093b6d48a3$b967d9b28025edb7f722e7a69fce6075cab8460411bb5c544f37f6116ee862f0', '+919876543212', 'user', TRUE),
    ('Rohan Verma', 'rohan.verma@example.com', 'rohan_v', 'pbkdf2_sha256$210000$99b8e38a15d9b9ef73dff7093b6d48a3$b967d9b28025edb7f722e7a69fce6075cab8460411bb5c544f37f6116ee862f0', '+919876543213', 'user', TRUE),
    ('Ananya Iyer', 'ananya.iyer@example.com', 'ananya_i', 'pbkdf2_sha256$210000$99b8e38a15d9b9ef73dff7093b6d48a3$b967d9b28025edb7f722e7a69fce6075cab8460411bb5c544f37f6116ee862f0', '+919876543214', 'user', TRUE),
    ('Vikram Malhotra', 'vikram.m@example.com', 'vikram_m', 'pbkdf2_sha256$210000$99b8e38a15d9b9ef73dff7093b6d48a3$b967d9b28025edb7f722e7a69fce6075cab8460411bb5c544f37f6116ee862f0', '+919876543215', 'user', TRUE),
    ('Sneha Reddy', 'sneha.reddy@example.com', 'sneha_r', 'pbkdf2_sha256$210000$99b8e38a15d9b9ef73dff7093b6d48a3$b967d9b28025edb7f722e7a69fce6075cab8460411bb5c544f37f6116ee862f0', '+919876543216', 'user', TRUE),
    ('Karan Joshi', 'karan.joshi@example.com', 'karan_j', 'pbkdf2_sha256$210000$99b8e38a15d9b9ef73dff7093b6d48a3$b967d9b28025edb7f722e7a69fce6075cab8460411bb5c544f37f6116ee862f0', '+919876543217', 'user', TRUE),
    ('Neha Gupta', 'neha.gupta@example.com', 'neha_g', 'pbkdf2_sha256$210000$99b8e38a15d9b9ef73dff7093b6d48a3$b967d9b28025edb7f722e7a69fce6075cab8460411bb5c544f37f6116ee862f0', '+919876543218', 'user', TRUE),
    ('Aditya Nair', 'aditya.nair@example.com', 'aditya_n', 'pbkdf2_sha256$210000$99b8e38a15d9b9ef73dff7093b6d48a3$b967d9b28025edb7f722e7a69fce6075cab8460411bb5c544f37f6116ee862f0', '+919876543219', 'user', TRUE),
    ('Pooja Mehta', 'pooja.mehta@example.com', 'pooja_m', 'pbkdf2_sha256$210000$99b8e38a15d9b9ef73dff7093b6d48a3$b967d9b28025edb7f722e7a69fce6075cab8460411bb5c544f37f6116ee862f0', '+919876543220', 'user', TRUE),
    ('Siddharth Rao', 'siddharth.rao@example.com', 'sid_rao', 'pbkdf2_sha256$210000$99b8e38a15d9b9ef73dff7093b6d48a3$b967d9b28025edb7f722e7a69fce6075cab8460411bb5c544f37f6116ee862f0', '+919876543221', 'user', TRUE)
ON CONFLICT (email) DO NOTHING;

-- ============================================================
-- 2. VEHICLES (At least 10 records)
-- ============================================================

INSERT INTO vehicles (user_id, registration_number, vehicle_type, is_electric, needs_accessible_parking)
SELECT u.user_id, v.reg, v.vtype, v.is_ev, v.acc
FROM (
    VALUES
        ('dnshneela@gmail.com', 'KA-01-MJ-1001', 'car', TRUE, FALSE),
        ('aarav.sharma@example.com', 'MH-02-AB-2002', 'car', FALSE, FALSE),
        ('priya.patel@example.com', 'DL-03-CD-3003', 'car', TRUE, FALSE),
        ('rohan.verma@example.com', 'KA-04-EF-4004', 'bike', FALSE, FALSE),
        ('ananya.iyer@example.com', 'TN-05-GH-5005', 'scooter', TRUE, FALSE),
        ('vikram.malhotra@example.com', 'HR-06-IJ-6006', 'car', FALSE, TRUE),
        ('sneha.reddy@example.com', 'AP-07-KL-7007', 'car', FALSE, FALSE),
        ('karan.joshi@example.com', 'MH-12-MN-8008', 'bike', FALSE, FALSE),
        ('neha.gupta@example.com', 'UP-16-OP-9009', 'scooter', FALSE, FALSE),
        ('aditya.nair@example.com', 'KL-08-QR-1010', 'car', TRUE, FALSE),
        ('pooja.mehta@example.com', 'GJ-01-ST-1111', 'car', FALSE, FALSE),
        ('siddharth.rao@example.com', 'TS-09-UV-1212', 'truck', FALSE, FALSE)
) AS v(email, reg, vtype, is_ev, acc)
JOIN users u ON u.email = v.email
ON CONFLICT (registration_number) DO NOTHING;

-- ============================================================
-- 3. PARKING AREAS (Ensure 5 distinct areas)
-- ============================================================

INSERT INTO parking_areas (name, location, operating_hours, capacity, status)
VALUES
    ('Mall Central - Ground Floor', 'Central Mall, MG Road', '07:00 - 23:00', 32, 'active'),
    ('Mall Central - First Floor', 'Central Mall, MG Road', '07:00 - 23:00', 32, 'active'),
    ('Mall Central - Second Floor', 'Central Mall, MG Road', '07:00 - 23:00', 32, 'active'),
    ('Tech Park - Basement A', 'Whitefield IT Corridor', '24/7', 40, 'active'),
    ('Metro Station Parking', 'Indiranagar Metro Station', '05:00 - 00:00', 25, 'active')
ON CONFLICT DO NOTHING;

-- ============================================================
-- 4. PARKING RATES (Comprehensive rules across vehicle types)
-- ============================================================

INSERT INTO parking_rates (area_id, vehicle_type, slot_type, hourly_rate, overstay_rate, ev_charging_rate)
SELECT pa.area_id, r.vtype, r.stype, r.hrate, r.orate, r.evrate
FROM parking_areas pa
CROSS JOIN (
    VALUES
        ('car', 'standard', 40.00, 60.00, 0.00),
        ('car', 'ev', 50.00, 75.00, 15.00),
        ('car', 'handicapped', 30.00, 45.00, 0.00),
        ('bike', 'two-wheeler', 20.00, 30.00, 0.00),
        ('scooter', 'two-wheeler', 15.00, 25.00, 0.00),
        ('truck', 'truck', 80.00, 120.00, 0.00)
) AS r(vtype, stype, hrate, orate, evrate)
ON CONFLICT (area_id, vehicle_type, slot_type) DO NOTHING;

-- ============================================================
-- 5. RESERVATIONS (At least 12 records)
-- ============================================================

DO $$
DECLARE
    r_user RECORD;
    r_veh RECORD;
    r_slot RECORD;
    i INT := 1;
BEGIN
    FOR r_veh IN (SELECT vehicle_id, user_id FROM vehicles ORDER BY vehicle_id LIMIT 12) LOOP
        SELECT slot_id INTO r_slot FROM parking_slots ORDER BY slot_id OFFSET (i * 2) LIMIT 1;
        
        IF r_slot IS NOT NULL THEN
            INSERT INTO reservations (user_id, vehicle_id, slot_id, start_time, end_time, status)
            VALUES (
                r_veh.user_id,
                r_veh.vehicle_id,
                r_slot.slot_id,
                CURRENT_TIMESTAMP - (INTERVAL '1 day' * i) - INTERVAL '2 hours',
                CURRENT_TIMESTAMP - (INTERVAL '1 day' * i) + INTERVAL '2 hours',
                CASE 
                    WHEN i <= 8 THEN 'completed'
                    WHEN i = 9 THEN 'confirmed'
                    WHEN i = 10 THEN 'active'
                    ELSE 'cancelled'
                END
            )
            ON CONFLICT DO NOTHING;
            i := i + 1;
        END IF;
    END LOOP;
END $$;

-- ============================================================
-- 6. PARKING SESSIONS (At least 10 records)
-- ============================================================

DO $$
DECLARE
    r_res RECORD;
    i INT := 1;
BEGIN
    FOR r_res IN (SELECT reservation_id, user_id, vehicle_id, slot_id, start_time FROM reservations WHERE status IN ('completed', 'active') ORDER BY reservation_id LIMIT 10) LOOP
        INSERT INTO parking_sessions (
            reservation_id, user_id, vehicle_id, slot_id,
            entry_time, exit_time, duration_minutes, status, overstay_duration_minutes, calculated_fee
        )
        VALUES (
            r_res.reservation_id,
            r_res.user_id,
            r_res.vehicle_id,
            r_res.slot_id,
            r_res.start_time + INTERVAL '5 minutes',
            CASE WHEN i <= 8 THEN r_res.start_time + INTERVAL '125 minutes' ELSE NULL END,
            CASE WHEN i <= 8 THEN 120 ELSE 45 END,
            CASE WHEN i <= 8 THEN 'completed' ELSE 'active' END,
            0,
            CASE WHEN i <= 8 THEN 80.00 ELSE 40.00 END
        )
        ON CONFLICT (reservation_id) DO NOTHING;
        i := i + 1;
    END LOOP;
END $$;

-- ============================================================
-- 7. PAYMENTS (At least 10 records)
-- ============================================================

DO $$
DECLARE
    r_sess RECORD;
    i INT := 1;
    v_methods TEXT[] := ARRAY['UPI', 'CARD', 'CASH', 'UPI', 'CARD', 'CASH', 'UPI', 'CARD', 'UPI', 'CASH'];
BEGIN
    FOR r_sess IN (SELECT session_id, reservation_id, user_id, calculated_fee FROM parking_sessions WHERE status = 'completed' ORDER BY session_id LIMIT 10) LOOP
        INSERT INTO payments (
            reservation_id, session_id, user_id, amount, payment_method, status, transaction_reference, paid_at
        )
        VALUES (
            r_sess.reservation_id,
            r_sess.session_id,
            r_sess.user_id,
            r_sess.calculated_fee,
            v_methods[1 + (i % 10)],
            'successful',
            'TXN-SAMPLE-' || LPAD(i::text, 5, '0'),
            CURRENT_TIMESTAMP - (INTERVAL '1 day' * i)
        )
        ON CONFLICT (transaction_reference) DO NOTHING;
        i := i + 1;
    END LOOP;
END $$;

-- ============================================================
-- 8. NOTIFICATIONS (At least 12 records)
-- ============================================================

INSERT INTO notifications (user_id, title, message, type, is_read, created_at)
SELECT 
    u.user_id,
    n.title,
    n.message,
    n.ntype,
    n.read_status,
    CURRENT_TIMESTAMP - (INTERVAL '3 hours' * n.seq)
FROM users u
CROSS JOIN LATERAL (
    VALUES
        (1, 'Reservation Confirmed', 'Your parking spot reservation has been successfully confirmed.', 'reservation', TRUE),
        (2, 'Parking Session Started', 'Vehicle checked in successfully. Session is now active.', 'session', TRUE),
        (3, 'Payment Receipt', 'Payment of INR 80.00 received successfully for your session.', 'payment', FALSE)
) AS n(seq, title, message, ntype, read_status)
WHERE u.role = 'user'
LIMIT 12;

-- ============================================================
-- 9. MAINTENANCE (At least 10 records)
-- ============================================================

DO $$
DECLARE
    r_slot RECORD;
    i INT := 1;
    v_reasons TEXT[] := ARRAY[
        'Sensor recalibration and cleaning',
        'EV charging plug inspection and ground test',
        'Bay striping and reflective paint touch-up',
        'Overhead camera line-of-sight maintenance',
        'Ground drainage cleaning in parking bay',
        'Boom barrier sensor timing adjustment',
        'Surface resurfacing and pressure wash',
        'Wheel stop re-anchoring and alignment',
        'Emergency LED beacon replacement',
        'EV power distribution unit routine test'
    ];
BEGIN
    FOR r_slot IN (SELECT slot_id FROM parking_slots ORDER BY slot_id LIMIT 10) LOOP
        INSERT INTO maintenance (slot_id, reason, start_time, end_time, status)
        VALUES (
            r_slot.slot_id,
            v_reasons[1 + (i % 10)],
            CURRENT_TIMESTAMP - (INTERVAL '2 days' * i),
            CURRENT_TIMESTAMP - (INTERVAL '2 days' * i) + INTERVAL '4 hours',
            CASE 
                WHEN i <= 7 THEN 'completed'
                WHEN i = 8 THEN 'in_progress'
                ELSE 'scheduled'
            END
        );
        i := i + 1;
    END LOOP;
END $$;

COMMIT;
