BEGIN TRANSACTION;


-- Root folder
INSERT INTO virtual_folders (id, name, is_private) VALUES (1, "Root", 0);


END TRANSACTION;
---------- Make sure to run DefaultTags.sql here

