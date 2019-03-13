-- This is ran within the main init transaction, so these aren't needed
-- All succeed or all will fail
--BEGIN TRANSACTION;

-- Version info 
CREATE TABLE version( number INTEGER );

CREATE TABLE pictures ( 
    id INTEGER PRIMARY KEY,
    -- Siqnature with libpuzzle for similarity checking
    signature TEXT
);    

-- Duplicate image checking support
CREATE TABLE picture_signature_words (
    picture_id INTEGER NOT NULL,
    sig_word TEXT NOT NULL,
    FOREIGN KEY (picture_id) REFERENCES pictures(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS position_sig ON picture_signature_words(picture_id, sig_word);


--COMMIT TRANSACTION;
