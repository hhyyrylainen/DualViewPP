-- Migration from database version 25 to 26 --

-- Stores images that user has manually confirmed to not be duplicates
CREATE TABLE ignored_duplicates (

    primary_image INTEGER NOT NULL,

    other_image INTEGER NOT NULL,

    UNIQUE(primary_image, other_image) ON CONFLICT IGNORE
);
