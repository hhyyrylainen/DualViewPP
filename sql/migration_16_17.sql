-- Migration from database version 16 to 17 --
BEGIN TRANSACTION;

DROP TABLE net_files;

CREATE TABLE net_files (

    id INTEGER PRIMARY KEY,
    
    -- Path to download this file from
    file_url TEXT NOT NULL,
    
    -- Referrer page to use, some sites require this
    page_referrer TEXT DEFAULT "",
    
    -- Name to use for this once downloaded
    preferred_name TEXT NOT NULL,

    -- Tags on this image. Contains ';' delimited list of tags
    tags_string TEXT,

    -- Part of a net gallery
    belongs_to_gallery INTEGER REFERENCES net_gallery(id) ON DELETE CASCADE
);


COMMIT TRANSACTION;
