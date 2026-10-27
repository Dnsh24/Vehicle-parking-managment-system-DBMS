-- Create live parking slots for existing areas that have capacity but no slots.
-- Rename every slot to <vehicle-symbol>-<number>, e.g. C-01 or E-03.
-- Safe to rerun; slot IDs and parking history stay unchanged.
BEGIN;

INSERT INTO parking_slots (area_id, slot_number, vehicle_type, slot_type, is_ev, status)
SELECT pa.area_id,
       'C-' || LPAD(g.slot_number::TEXT, 2, '0'),
       'car',
       'standard',
       FALSE,
       'available'
FROM parking_areas pa
CROSS JOIN LATERAL generate_series(1, pa.capacity) AS g(slot_number)
WHERE NOT EXISTS (
    SELECT 1 FROM parking_slots ps WHERE ps.area_id = pa.area_id
);

-- Free old labels before applying the new labels under the area/number unique key.
UPDATE parking_slots SET slot_number = 'tmp-' || slot_id::TEXT;

WITH classified_slots AS (
    SELECT ps.slot_id,
           ps.area_id,
           CASE
               WHEN ps.is_ev OR ps.slot_type ILIKE '%ev%' OR ps.slot_type ILIKE '%electric%'
                    OR ps.slot_type ILIKE '%charg%' THEN 'E'
               WHEN ps.slot_type ILIKE '%handicap%' OR ps.slot_type ILIKE '%accessible%' THEN 'H'
               WHEN ps.vehicle_type IN ('bike', 'scooter') OR ps.slot_type ILIKE '%two-wheeler%' THEN 'B'
               WHEN ps.vehicle_type IN ('truck', 'bus') OR ps.slot_type = 'truck' THEN 'T'
               ELSE 'C'
           END AS vehicle_symbol
    FROM parking_slots ps
), numbered_slots AS (
    SELECT slot_id,
           vehicle_symbol,
           row_number() OVER (
               PARTITION BY area_id, vehicle_symbol
               ORDER BY slot_id
           ) AS category_number
    FROM classified_slots
)
UPDATE parking_slots ps
SET slot_number = numbered_slots.vehicle_symbol || '-' ||
                  LPAD(numbered_slots.category_number::TEXT, 2, '0')
FROM numbered_slots
WHERE ps.slot_id = numbered_slots.slot_id;

COMMIT;
