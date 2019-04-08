-- Migration from database version 24 to 25 --

ALTER TABLE action_history ADD COLUMN description TEXT;
