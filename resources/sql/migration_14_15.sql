-- Migration from database version 14 to 15 --
BEGIN TRANSACTION;

-- Clear tables common_composite_tags, composite_tag_modifiers
DROP TABLE common_composite_tags;
DROP TABLE composite_tag_modifiers;

-- Recreate the tables
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

CREATE TABLE composite_tag_modifiers (
    
    composite INTEGER REFERENCES common_composite_tags(id) ON DELETE CASCADE,
    modifier INTEGER REFERENCES tag_modifiers(id) ON DELETE CASCADE
);

-- Insert new values
INSERT INTO common_composite_tags (tag_string, actual_tag) VALUES ("*grab", 0);
        
INSERT INTO composite_tag_modifiers (composite, modifier) VALUES ( (SELECT last_insert_rowid()) , (SELECT id FROM tag_modifiers WHERE name="grabbing") );

-- Create new tables: tag_modifier_aliases tag_super_aliases
-- Super aliases. These allow a matching tag string to expand to the expanded form
CREATE TABLE tag_super_aliases (

       -- The input the text must match
       alias TEXT UNIQUE COLLATE NOCASE,
       
       -- The text the match must expand to
       expanded TEXT
);

-- Tag Modifier alias. Allows multiple names for a modifier
CREATE TABLE tag_modifier_aliases (
    
    -- Name of the alias
    name TEXT UNIQUE COLLATE NOCASE,
    -- The Modifier this name means
    meant_modifier INTEGER REFERENCES tag_modifiers(id) ON DELETE CASCADE
);

-- Insert new values
-- Redhead
INSERT INTO tag_super_aliases (alias, expanded) VALUES ("redhead", "red hair");

-- Brunette
INSERT INTO tag_super_aliases (alias, expanded) VALUES ("brunette", "brown hair");

-- Blonde
INSERT INTO tag_super_aliases (alias, expanded) VALUES ("blonde", "blonde hair");



COMMIT TRANSACTION;

