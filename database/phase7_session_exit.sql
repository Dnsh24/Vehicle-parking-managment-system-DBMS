BEGIN;

-- Ensure index exists on payments(session_id) for session fee and payment lookups
CREATE INDEX IF NOT EXISTS idx_payments_session_id
    ON payments(session_id);

COMMIT;

