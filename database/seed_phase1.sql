-- Phase 1 development seed data
-- Three Mall Central floors, 32 slots per floor, named vehicle-type-number.
-- This script is safe to run repeatedly for these area and slot names.
BEGIN;

INSERT INTO parking_areas (
    name,
    location,
    operating_hours,
    capacity,
    status
)
SELECT seed.name, 'Mall Central', '07:00 - 22:00', 32, 'active'
FROM (
    VALUES
        ('Mall Central - Ground Floor'),
        ('Mall Central - First Floor'),
        ('Mall Central - Second Floor')
) AS seed(name)
WHERE NOT EXISTS (
    SELECT 1
    FROM parking_areas existing
    WHERE existing.name = seed.name
);

WITH floor_data AS (
    SELECT *
    FROM (
        VALUES
            ('Mall Central - Ground Floor'),
            ('Mall Central - First Floor'),
            ('Mall Central - Second Floor')
    ) AS floors(name)
),
slot_data AS (
    SELECT
        areas.area_id,
        slot_number,
        CASE
            WHEN slot_number <= 4 THEN 'truck'
            WHEN slot_number <= 14 THEN 'bike'
            ELSE 'car'
        END AS vehicle_type,
        CASE
            WHEN slot_number <= 4 THEN 'truck'
            WHEN slot_number <= 14 THEN 'two-wheeler'
            WHEN slot_number <= 26 THEN 'standard'
            WHEN slot_number <= 29 THEN 'ev'
            ELSE 'handicapped'
        END AS slot_type,
        CASE
            WHEN slot_number <= 4 THEN 'T'
            WHEN slot_number <= 14 THEN 'B'
            WHEN slot_number <= 26 THEN 'C'
            WHEN slot_number <= 29 THEN 'E'
            ELSE 'H'
        END AS type_symbol,
        slot_number BETWEEN 27 AND 29 AS is_ev,
        CASE
            WHEN slot_number IN (2, 8, 19, 27) THEN 'occupied'
            WHEN slot_number IN (5, 16, 30) THEN 'reserved'
            WHEN slot_number IN (12, 25) THEN 'maintenance'
            WHEN slot_number IN (4, 22, 32) THEN 'unavailable'
            ELSE 'available'
        END AS status
    FROM floor_data
    JOIN parking_areas areas ON areas.name = floor_data.name
    CROSS JOIN generate_series(1, 32) AS slots(slot_number)
), formatted_slot_data AS (
    SELECT slot_data.*,
           row_number() OVER (
               PARTITION BY slot_data.area_id, slot_data.type_symbol
               ORDER BY slot_data.slot_number
           ) AS type_slot_number
    FROM slot_data
)
INSERT INTO parking_slots (
    area_id,
    slot_number,
    vehicle_type,
    slot_type,
    is_ev,
    status
)
SELECT
    slot_data.area_id,
    slot_data.type_symbol || '-' ||
        LPAD(slot_data.type_slot_number::TEXT, 2, '0'),
    slot_data.vehicle_type,
    slot_data.slot_type,
    slot_data.is_ev,
    slot_data.status
FROM formatted_slot_data slot_data
WHERE NOT EXISTS (
    SELECT 1
    FROM parking_slots existing
    WHERE existing.area_id = slot_data.area_id
      AND existing.slot_number =
          slot_data.type_symbol || '-' ||
          LPAD(slot_data.type_slot_number::TEXT, 2, '0')
);

COMMIT;
