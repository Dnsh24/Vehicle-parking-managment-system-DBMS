CREATE INDEX IF NOT EXISTS idx_notifications_user_created
    ON notifications(user_id, created_at DESC, notification_id DESC);

CREATE INDEX IF NOT EXISTS idx_notifications_unread_user
    ON notifications(user_id)
    WHERE is_read = FALSE;
