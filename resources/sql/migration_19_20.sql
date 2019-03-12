-- Migration from database version 18 to 19 --

-- This is a really hacky thing to do
-- but apparently recommended by SQLite manual 
BEGIN TRANSACTION;

ALTER TABLE pictures ADD COLUMN signature TEXT;

-- Duplicate image checking support
CREATE TABLE picture_signature_words (
    picture_id INTEGER NOT NULL,
    sig_word TEXT NOT NULL,
    FOREIGN KEY (picture_id) REFERENCES pictures(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS position_sig ON picture_signature_words(picture_id, sig_word);

COMMIT TRANSACTION;

       
