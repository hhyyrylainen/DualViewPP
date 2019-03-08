-- Migration from database version 15 to 16 --
BEGIN TRANSACTION;

ALTER TABLE applied_tag_combine RENAME TO applied_tag_combine_old;

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

INSERT INTO applied_tag_combine (tag_left, tag_right, combined_with) SELECT
       tag_left, tag_right, combined_with FROM applied_tag_combine_old;

DROP TABLE applied_tag_combine_old;

COMMIT TRANSACTION;
