BEGIN TRANSACTION;


-- Default value insertions
INSERT INTO collections (id, name) VALUES (1, "Uncategorized");
-- Uncategorized for private
INSERT INTO collections (id, name, is_private) VALUES (2, "PrivateRandom", 1);

INSERT INTO collections (name) VALUES ("Backgrounds");

-- Root folder
INSERT INTO virtual_folders (id, name, is_private) VALUES (1, "Root", 0);

-- Add default collections to root folder
INSERT INTO folder_collection (parent, child) VALUES (1, (SELECT id FROM collections WHERE name = "Uncategorized"));
INSERT INTO folder_collection (parent, child) VALUES (1, (SELECT id FROM collections WHERE name = "PrivateRandom"));
INSERT INTO folder_collection (parent, child) VALUES (1, (SELECT id FROM collections WHERE name = "Backgrounds"));


END TRANSACTION;
---------- Make sure to run DefaultTags.sql here

