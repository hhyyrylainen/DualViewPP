-- Migration from database version 14 to 15 --
BEGIN TRANSACTION;

-- Clear tables common_composite_tags, composite_tag_modifiers
DELETE FROM common_composite_tags;
DELETE FROM composite_tag_modifiers;

-- Add the replacement values
INSERT INTO common_composite_tags (tag_string, actual_tag) VALUES ("*grab", 0);
INSERT INTO composite_tag_modifiers (composite, modifier) VALUES ((SELECT last_insert_rowid()), (SELECT id FROM tag_modifiers WHERE name="grabbing"));

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

