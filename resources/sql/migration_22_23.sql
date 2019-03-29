-- Migration from database version 22 to 23 --
BEGIN TRANSACTION;

ALTER TABLE pictures ADD COLUMN deleted INTEGER;
ALTER TABLE image_region ADD COLUMN deleted INTEGER;
ALTER TABLE tags ADD COLUMN deleted INTEGER;
ALTER TABLE tag_modifiers ADD COLUMN deleted INTEGER;
ALTER TABLE common_composite_tags ADD COLUMN deleted INTEGER;
ALTER TABLE collections ADD COLUMN deleted INTEGER;
ALTER TABLE virtual_folders ADD COLUMN deleted INTEGER;
ALTER TABLE net_gallery ADD COLUMN deleted INTEGER;

COMMIT TRANSACTION;
