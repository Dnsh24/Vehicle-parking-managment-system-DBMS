BEGIN;

-- 1. Ensure planned_end_time column exists on parking_assignments
ALTER TABLE parking_assignments
    ADD COLUMN IF NOT EXISTS planned_end_time TIMESTAMPTZ;

-- 2. Safely backfill existing assignments where planned_end_time is null
-- Default to 2 hours after parked_at (or checkout_at if earlier)
UPDATE parking_assignments
SET planned_end_time = COALESCE(checkout_at, parked_at + INTERVAL '2 hours')
WHERE planned_end_time IS NULL;

-- 3. Add check constraint to ensure planned_end_time >= parked_at if not present
DO $$
BEGIN
    IF NOT EXISTS (
        SELECT 1 FROM pg_constraint WHERE conname = 'parking_assignments_planned_end_check'
    ) THEN
        ALTER TABLE parking_assignments
            ADD CONSTRAINT parking_assignments_planned_end_check
            CHECK (planned_end_time IS NULL OR planned_end_time >= parked_at);
    END IF;
END $$;

-- 4. Create an index on planned_end_time for active assignments
CREATE INDEX IF NOT EXISTS idx_parking_assignments_active_planned_end
    ON parking_assignments(planned_end_time)
    WHERE checkout_at IS NULL;

-- 5. Allow payments for direct parking assignments without reservations
ALTER TABLE payments ALTER COLUMN reservation_id DROP NOT NULL;

COMMIT;

