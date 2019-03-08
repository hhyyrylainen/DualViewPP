-- Migration from database version 18 to 19 --

-- This is a really hacky thing to do
-- but apparently recommended by SQLite manual 
PRAGMA foreign_keys = OFF;

BEGIN TRANSACTION;

ALTER TABLE pictures RENAME TO pictures_old;

-- Main pictures table
CREATE TABLE pictures ( 
    id INTEGER PRIMARY KEY AUTOINCREMENT, 
    -- Path from which this image can be loaded
    relative_path TEXT,
    -- When importing the size is determined
    width INTEGER, 
    height INTEGER, 
    -- Friendly name, shown in image browser
    name TEXT, 
    extension TEXT, 
    add_date TEXT, 
    last_view TEXT,
    -- If 1 only visible in private mode
    is_private INTEGER DEFAULT 0,
    
    from_file TEXT DEFAULT "", 
    
    -- Hash of the file
    file_hash TEXT NOT NULL UNIQUE
);

INSERT INTO pictures SELECT * FROM pictures_old;

DROP TABLE pictures_old;

-- This should actually be output and checked with a regex
PRAGMA foreign_key_check;

COMMIT TRANSACTION;

PRAGMA foreign_keys = ON;



