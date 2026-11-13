-- ChildRepository.findByOwner is used by account lists and every subscribed
-- admin's device snapshot. Run once on existing PostgreSQL deployments.
-- PostgreSQL allows IF NOT EXISTS, so repeating this migration is safe.
CREATE INDEX IF NOT EXISTS idx_child_user_id ON child (user_id);
