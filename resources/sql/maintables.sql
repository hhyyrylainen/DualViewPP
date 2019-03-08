-- This is ran within the main init transaction, so these aren't needed
-- All succeed or all will fail
--BEGIN TRANSACTION;

-- Version info 
CREATE TABLE version( number INTEGER );

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

-- This is replaced by forcing file_hash to be unique
--CREATE INDEX pictures_file_hash ON pictures (file_hash);

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

-- Modifies tags, like colours and numbers
CREATE TABLE tag_modifiers (
    
    id INTEGER PRIMARY KEY AUTOINCREMENT, 
    
    name TEXT UNIQUE,
    
    -- If 1 only visible in private mode
    -- Will automatically set any applied tags this is attached to be private
    is_private INTEGER DEFAULT 0,
    
    -- Similar to category in tags, basically the type of modifier, some examples:
    -- colour, number, intensity
    description TEXT
);

-- Tag Modifier alias. Allows multiple names for a modifier
CREATE TABLE tag_modifier_aliases (
    
    -- Name of the alias
    name TEXT UNIQUE COLLATE NOCASE,
    -- The Modifier this name means
    meant_modifier INTEGER REFERENCES tag_modifiers(id) ON DELETE CASCADE
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

-- Detect tags that should actually be modified tags
CREATE TABLE common_composite_tags (
    
    id INTEGER PRIMARY KEY AUTOINCREMENT, 
    
    -- The text tto match this detection
    -- The string may contain * to indicate that it may be any tag, 
    -- for example "wet*" would match any tag that has 'wet' in front of an existing tag
    tag_string TEXT UNIQUE,
    
    -- The actual tag meant
    -- May be 0 if * is used
    actual_tag INTEGER NOT NULL
    
);

-- The modifiers to apply to the above table detection rule
CREATE TABLE composite_tag_modifiers (
    
    composite INTEGER REFERENCES common_composite_tags(id) ON DELETE CASCADE,
    modifier INTEGER REFERENCES tag_modifiers(id) ON DELETE CASCADE
);

-- Tag alias. Allows multiple names for a tag
CREATE TABLE tag_aliases (
    
    -- Name of the alias
    name TEXT UNIQUE COLLATE NOCASE,
    -- The tag this name means
    meant_tag INTEGER REFERENCES tags(id) ON DELETE CASCADE
);

-- Unhelpful tags can be blacklisted
CREATE TABLE tag_blacklist (
    
    -- Name of tag
    name TEXT UNIQUE COLLATE NOCASE,
    -- Reason this is blacklisted
    reason TEXT DEFAULT "Unhelpful tag, use something else"
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

-- Super aliases. These allow a matching tag string to expand to the expanded form
CREATE TABLE tag_super_aliases (

       -- The input the text must match
       alias TEXT UNIQUE COLLATE NOCASE,
       
       -- The text the match must expand to
       expanded TEXT
);



-- Tag applied to an image
CREATE TABLE image_tag ( 
    image NOT NULL REFERENCES pictures(id) ON DELETE CASCADE,
    tag NOT NULL REFERENCES applied_tag(id) ON DELETE CASCADE,
    UNIQUE (image, tag) ON CONFLICT IGNORE
);

-- Tag applied to a collection
CREATE TABLE collection_tag (
    collection NOT NULL REFERENCES collections(id) ON DELETE CASCADE,
    tag NOT NULL REFERENCES applied_tag(id) ON DELETE CASCADE,
    UNIQUE (collection, tag) ON CONFLICT IGNORE
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

-- Image belonging to a collection
CREATE TABLE collection_image ( 
    -- Orders the images in a collection
    show_order INTEGER DEFAULT 1,
    image NOT NULL REFERENCES pictures(id) ON DELETE CASCADE,
    collection NOT NULL REFERENCES collections(id) ON DELETE CASCADE,
    UNIQUE (image, collection) ON CONFLICT IGNORE
);

-- Virtual folders table. These contain collections and other folders. Base structure for browsing
-- folders can be added to multiple folders so there is no simple parent folder other than that all folders
-- must eventually be children of the Root folder
-- Paths are created like this Root/{name}/{name}/{collection}
-- '/' characters are disallowed in folder names convert to | 
CREATE TABLE virtual_folders (
    
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    
    -- Name of the folder
    name TEXT NOT NULL CHECK (name NOT LIKE "%/%"),
    
    -- Is a private folder, only visible in private mode if set to 1
    is_private INTEGER DEFAULT 0
);

-- Collection added to a virtual folder
CREATE TABLE folder_collection (
    
    parent INTEGER NOT NULL REFERENCES virtual_folders(id) ON DELETE RESTRICT,
    child INTEGER NOT NULL REFERENCES collections(id) ON DELETE CASCADE,
    UNIQUE (parent, child) ON CONFLICT IGNORE
);

-- Folder within a folder. Disallows setting inside itself
CREATE TABLE folder_folder (

    parent INTEGER NOT NULL REFERENCES virtual_folders(id) ON DELETE RESTRICT,
    child INTEGER NOT NULL CHECK (child IS NOT parent) REFERENCES virtual_folders(id) ON DELETE CASCADE,
    UNIQUE (parent, child) ON CONFLICT IGNORE
);

-- Internet download things --

-- Resource file found on the internet
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

-- An online gallery
CREATE TABLE net_gallery (
    
    id INTEGER PRIMARY KEY,
    
    -- Main url
    gallery_url TEXT NOT NULL,

    -- Folder target path
    target_path TEXT DEFAULT "",
    
    -- Name of gallery (if already received a response to http request)
    -- When saving download_settings strip settings should be applied before saving
    gallery_name TEXT,
    
    -- Currently scanned page
    currently_scanned TEXT,
    
    -- Is downloaded successfully
    is_downloaded INTEGER DEFAULT 0,
    
    -- Contains ';' delimited list of tags
    tags_string TEXT
);

-- Plugin management tables
-- This table is used to initialize all plugins once
CREATE TABLE activated_db_plugins (

       plugin_uuid TEXT PRIMARY KEY
);


--COMMIT TRANSACTION;
