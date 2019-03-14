-- Migration from database version 21 to 22 --
-- This fixes pictures_old still being referenced all over

PRAGMA foreign_keys = OFF;


BEGIN TRANSACTION;

--
-- Rename old tables
--

ALTER TABLE ratings RENAME TO ratings_old;
ALTER TABLE image_region RENAME TO image_region_old;
ALTER TABLE tags RENAME TO tags_old;
ALTER TABLE applied_tag RENAME TO applied_tag_old;
ALTER TABLE applied_tag_combine RENAME TO applied_tag_combine_old;
ALTER TABLE applied_tag_modifier RENAME TO applied_tag_modifier_old;
ALTER TABLE tag_aliases RENAME TO tag_aliases_old;
ALTER TABLE tag_implies RENAME TO tag_implies_old;
ALTER TABLE image_tag RENAME TO image_tag_old;
ALTER TABLE collections RENAME TO collections_old;
ALTER TABLE collection_tag RENAME TO collection_tag_old;
ALTER TABLE collection_image RENAME TO collection_image_old;
ALTER TABLE folder_collection RENAME TO folder_collection_old;

--
-- Create new tables
--


-- Stores rating information about collections, images and whatever else wants to have ratings, too
CREATE TABLE ratings ( 
    id INTEGER PRIMARY KEY AUTOINCREMENT, 
    -- 1 If added to favorites
    favorited INTEGER DEFAULT 0, 
    -- Rating of this image -1 is not set 0 - 5 is rated
    stars INTEGER DEFAULT -1, 
    
    -- Rated image
    image INTEGER UNIQUE REFERENCES pictures(id) ON DELETE CASCADE
);

-- Region of an image, used to apply tags to specific regions
CREATE TABLE image_region ( 
    id INTEGER PRIMARY KEY AUTOINCREMENT, 
    -- Region, if all are 0 is the entire image
    area_top INTEGER DEFAULT 0,
    area_left INTEGER DEFAULT 0,
    area_right INTEGER DEFAULT 0,
    area_bottom INTEGER DEFAULT 0,
    
    -- Multiframe images
    start_frame INTEGER DEFAULT 0,
    end_frame INTEGER DEFAULT 0,
    
    parent_image INTEGER NOT NULL REFERENCES pictures(id) ON DELETE CASCADE
);

-- All defined tags
CREATE TABLE tags ( 
    id INTEGER PRIMARY KEY AUTOINCREMENT, 
    -- Showed when this tag is applied to something
    name TEXT UNIQUE COLLATE NOCASE, 
    -- Category of the tag. Used to narrow down what the tag is
    -- Possible values: 
    -- 0 - describes an object or a character in the image
    -- 1 - Tags a character or an person in the image
    -- 2 - Tags something that's not immediately visible from the image or relates to something meta, like captions
    -- 3 - Tags the series or universe this image belongs to, for example star wars. Or another loosely defined series
    -- 4 - Tags an action that is being performed
    -- 5 - Tags the images level of "helpfullness / quality" only one of a tag of this type should apply to an image
    category INTEGER DEFAULT 0,
    -- Long description showed when howered
    description TEXT, 
    -- If 1 only visible in private mode
    -- Will automatically set any images this is attached to be private
    is_private INTEGER DEFAULT 0,
    -- Sample image shown when applying this tag, used to show what the tag means
    example_image_region INTEGER DEFAULT NULL REFERENCES image_region(id) ON DELETE SET NULL ON UPDATE RESTRICT
);

-- A tag that can be applied
CREATE TABLE applied_tag (
    
    id INTEGER PRIMARY KEY AUTOINCREMENT, 
    tag INTEGER REFERENCES tags(id) ON DELETE CASCADE
);

-- Combined tags of form {tag} {preposition / action} {tag}
CREATE TABLE applied_tag_combine (

    -- Each tag can have only one combine
    tag_left INTEGER UNIQUE REFERENCES applied_tag(id) ON DELETE CASCADE,
    tag_right INTEGER REFERENCES applied_tag(id) ON DELETE CASCADE,
    
    -- The word in between, like on, in, covering
    combined_with TEXT NOT NULL,

    -- Only one combine between 2 tags
    UNIQUE (tag_left, tag_right)
);

-- A modifier applied to an applied tag
CREATE TABLE applied_tag_modifier (

    to_tag INTEGER REFERENCES applied_tag(id) ON DELETE CASCADE,
    modifier INTEGER REFERENCES tag_modifiers(id) ON DELETE RESTRICT,
    
    UNIQUE (to_tag, modifier)
);

-- Tag alias. Allows multiple names for a tag
CREATE TABLE tag_aliases (
    
    -- Name of the alias
    name TEXT UNIQUE COLLATE NOCASE,
    -- The tag this name means
    meant_tag INTEGER REFERENCES tags(id) ON DELETE CASCADE
);

-- Implicit tags, applies a tag if another is already applied
CREATE TABLE tag_implies (
    
    -- Primary tag
    primary_tag INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    
    -- Tag to apply when primary is detected
    to_apply INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    
    -- The pair must be unique
    UNIQUE (primary_tag, to_apply)
);


-- Tag applied to an image
CREATE TABLE image_tag ( 
    image NOT NULL REFERENCES pictures(id) ON DELETE CASCADE,
    tag NOT NULL REFERENCES applied_tag(id) ON DELETE CASCADE,
    UNIQUE (image, tag) ON CONFLICT IGNORE
);

-- Collections table
-- '/' characters are disallowed in collection names convert to | 
CREATE TABLE collections ( 
    id INTEGER PRIMARY KEY AUTOINCREMENT, 
    -- Name showed in collection browser. Has to be unique
    name TEXT UNIQUE CHECK (name NOT LIKE "%/%"),
    
    add_date TEXT, 
    modify_date TEXT, 
    last_view TEXT,
    -- If 1 only visible in private mode
    is_private INTEGER DEFAULT 0,
    
    -- Preview image, if selected otherwise first image returned by the database
    preview_image INTEGER DEFAULT NULL REFERENCES pictures(id) ON DELETE SET NULL
);

-- Tag applied to a collection
CREATE TABLE collection_tag (
    collection NOT NULL REFERENCES collections(id) ON DELETE CASCADE,
    tag NOT NULL REFERENCES applied_tag(id) ON DELETE CASCADE,
    UNIQUE (collection, tag) ON CONFLICT IGNORE
);

-- Image belonging to a collection
CREATE TABLE collection_image ( 
    -- Orders the images in a collection
    show_order INTEGER DEFAULT 1,
    image NOT NULL REFERENCES pictures(id) ON DELETE CASCADE,
    collection NOT NULL REFERENCES collections(id) ON DELETE CASCADE,
    UNIQUE (image, collection) ON CONFLICT IGNORE
);

-- Collection added to a virtual folder
CREATE TABLE folder_collection (
    
    parent INTEGER NOT NULL REFERENCES virtual_folders(id) ON DELETE RESTRICT,
    child INTEGER NOT NULL REFERENCES collections(id) ON DELETE CASCADE,
    UNIQUE (parent, child) ON CONFLICT IGNORE
);

--
-- Copy data
--
INSERT INTO ratings SELECT id, favorited, stars, image FROM ratings_old;
INSERT INTO image_region SELECT * FROM image_region_old;
INSERT INTO tags SELECT * FROM tags_old;
INSERT INTO applied_tag SELECT * FROM applied_tag_old;
INSERT INTO applied_tag_combine SELECT * FROM applied_tag_combine_old;
INSERT INTO applied_tag_modifier SELECT * FROM applied_tag_modifier_old;
INSERT INTO tag_aliases SELECT * FROM tag_aliases_old;
INSERT INTO tag_implies SELECT * FROM tag_implies_old;
INSERT INTO image_tag SELECT * FROM image_tag_old;
INSERT INTO collections SELECT * FROM collections_old;
INSERT INTO collection_tag SELECT * FROM collection_tag_old;
INSERT INTO collection_image SELECT * FROM collection_image_old;
INSERT INTO folder_collection SELECT * FROM folder_collection_old;

-- Drop old tables
DROP TABLE ratings_old;
DROP TABLE image_region_old;
DROP TABLE tags_old;
DROP TABLE applied_tag_old;
DROP TABLE applied_tag_combine_old;
DROP TABLE applied_tag_modifier_old;
DROP TABLE tag_aliases_old;
DROP TABLE tag_implies_old;
DROP TABLE image_tag_old;
DROP TABLE collections_old;
DROP TABLE collection_tag_old;
DROP TABLE collection_image_old;
DROP TABLE folder_collection_old;

-- This should actually be output and checked with a regex
PRAGMA foreign_key_check;

COMMIT TRANSACTION;

PRAGMA foreign_keys = ON;



