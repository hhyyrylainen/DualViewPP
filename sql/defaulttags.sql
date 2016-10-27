BEGIN TRANSACTION;

-- Blacklisted tags
INSERT INTO tag_blacklist (name, reason) VALUES ("fun", "Doesn't really describe what it is");


-- Default tags --
-- Category 0
INSERT INTO tags (name, description, category, is_private) VALUES ("female", "A distinctively feminine character", 0, 0);
INSERT INTO tags (name, description, category, is_private) VALUES ("male", "A distinctively male character", 0, 0);

-- Some imported tags, but not too many to take forever to initialize databases
INSERT INTO tags (name, description, category, is_private) VALUES ("clothing", "This tag is an umbrella tag for all the items of clothing, like the ones you're wearing right now!At least, we hope you are...", 0, 0);
INSERT INTO tag_aliases (name, meant_tag) VALUES ("clothes", (SELECT id FROM tags WHERE name = "clothing"));
INSERT INTO tag_aliases (name, meant_tag) VALUES ("cloths", (SELECT id FROM tags WHERE name = "clothing"));

INSERT INTO tags (name, description, category, is_private) VALUES ("uniform", "A uniform is a set of standard clothing worn by members of an organization while participating in that organization's activities. Modern uniforms are worn by armed forces  and paramilitary organizations such as police, emergency services, security guards, in some workplaces and schools, and by inmates in prisons. In some countries, some other officials also wear uniforms in their duties; such is the case of the Commissioned Corps of the United States Public Health Service or the French prefects. For some public groups, such as police, it is illegal for non members to wear the uniform. Other uniforms are trade dressed (such as the brown uniforms of UPS).", 0, 0);
-- Implied tag 'uniform => clothing
INSERT INTO tag_implies (primary_tag, to_apply) VALUES ((SELECT id FROM tags WHERE name = "uniform"), (SELECT id FROM tags WHERE name = "clothing"));

-- Tag baseball uniform 
INSERT INTO tags (name, description, category, is_private) VALUES ("baseball uniform", "A uniform worn by those who are playing baseball. ", 0, 0);
-- Implied tag baseball uniform => uniform 
INSERT INTO tag_implies (primary_tag, to_apply) VALUES ((SELECT id FROM tags WHERE name = "baseball uniform"), (SELECT id FROM tags WHERE name = "uniform"));


-- Category 1
INSERT INTO tags (name, description, category, is_private) VALUES ("emma watson", "The actress", 1, 0);
INSERT INTO tags (name, description, category, is_private) VALUES ("taylor swift", "Pop Singer", 1, 0);
INSERT INTO tags (name, description, category, is_private) VALUES ("pewdiepie", "Youtube Star", 1, 0);
INSERT INTO tags (name, description, category, is_private) VALUES ("ariana grande", "Pop Singer", 1, 0);
INSERT INTO tags (name, description, category, is_private) VALUES ("selena gomez", "Pop Singer", 1, 0);
INSERT INTO tags (name, description, category, is_private) VALUES ("beyoncé knowles", "Pop Singer", 1, 0);
INSERT INTO tags (name, description, category, is_private) VALUES ("kim kardashian", "Reality Star", 1, 0);
INSERT INTO tags (name, description, category, is_private) VALUES ("shawn mendes", "Pop Singer", 1, 0);


-- Category 2
INSERT INTO tags (name, description, category, is_private) VALUES ("behind the scenes", "Behind the scenes image for example from a movie set", 2, 0);
INSERT INTO tags (name, description, category, is_private) VALUES ("captions", "The image has text over it", 2, 0);
INSERT INTO tags (name, description, category, is_private) VALUES ("watermark", "The image is watermarked", 2, 0);
INSERT INTO tags (name, description, category, is_private) VALUES ("drawn", "Not a photo, cgi or hand-drawn", 2, 0);
INSERT INTO tags (name, description, category, is_private) VALUES ("anime", "yep, it's heresy ... I mean it's anime", 2, 0);

INSERT INTO tags (name, description, category, is_private) VALUES ("tagme", "Image is untagged", 2, 0);

-- Category 3
INSERT INTO tags (name, description, category, is_private) VALUES ("eve online", "The MMORPG", 3, 0);
INSERT INTO tags (name, description, category, is_private) VALUES ("star wars", "The classic", 3, 0);

-- Category 4
INSERT INTO tags (name, description, category, is_private) VALUES ("looking at viewer", "A character is looking at the camera", 4, 0);

-- Category 5
INSERT INTO tags (name, description, category, is_private) VALUES ("lame", "The image is clearly lame", 5, 0);
INSERT INTO tags (name, description, category, is_private) VALUES ("safe", "This image is safe to view at work", 5, 0);


-- Aliases --
INSERT INTO tag_aliases (name, meant_tag) VALUES ("EVE", (SELECT id FROM tags WHERE name = "EVE Online"));

INSERT INTO tag_aliases (name, meant_tag) VALUES ("Anime / Cartoon", (SELECT id FROM tags WHERE name = "drawn"));

INSERT INTO tag_aliases (name, meant_tag) VALUES ("beyonce", (SELECT id FROM tags WHERE name = "beyoncé knowles"));
INSERT INTO tag_aliases (name, meant_tag) VALUES ("beyonce knowles", (SELECT id FROM tags WHERE name = "beyoncé knowles"));

-- Implicit tag apply --
INSERT INTO tag_implies (primary_tag, to_apply) VALUES ( 
    (SELECT id FROM tags WHERE name = "anime"), (SELECT id FROM tags WHERE name = "drawn"));
    
-- Tag modifiers
-- Numbers
INSERT INTO tag_modifiers (name, description) VALUES ("0", "Number");
INSERT INTO tag_modifiers (name, description) VALUES ("1", "Number");
INSERT INTO tag_modifiers (name, description) VALUES ("2", "Number");
INSERT INTO tag_modifiers (name, description) VALUES ("3", "Number");
INSERT INTO tag_modifiers (name, description) VALUES ("4", "Number");
INSERT INTO tag_modifiers (name, description) VALUES ("5", "Number");
INSERT INTO tag_modifiers (name, description) VALUES ("6", "Number");
INSERT INTO tag_modifiers (name, description) VALUES ("7", "Number");
INSERT INTO tag_modifiers (name, description) VALUES ("8", "Number");
INSERT INTO tag_modifiers (name, description) VALUES ("9", "Number");
INSERT INTO tag_modifiers (name, description) VALUES ("10", "Number");
INSERT INTO tag_modifiers (name, description) VALUES ("lots", "Number");

-- Colours
INSERT INTO tag_modifiers (name, description) VALUES ("red", "Colour");
INSERT INTO tag_modifiers (name, description) VALUES ("blue", "Colour");
INSERT INTO tag_modifiers (name, description) VALUES ("black", "Colour");
INSERT INTO tag_modifiers (name, description) VALUES ("blonde", "Colour");
INSERT INTO tag_modifiers (name, description) VALUES ("brown", "Colour");
INSERT INTO tag_modifiers (name, description) VALUES ("green", "Colour");
INSERT INTO tag_modifiers (name, description) VALUES ("grey", "Colour");
INSERT INTO tag_modifiers (name, description) VALUES ("orange", "Colour");
INSERT INTO tag_modifiers (name, description) VALUES ("pink", "Colour");
INSERT INTO tag_modifiers (name, description) VALUES ("purple", "Colour");
INSERT INTO tag_modifiers (name, description) VALUES ("white", "Colour"); 
INSERT INTO tag_modifiers (name, description) VALUES ("silver", "Colour");
INSERT INTO tag_modifiers (name, description) VALUES ("yellow", "Colour");
INSERT INTO tag_modifiers (name, description) VALUES ("teal", "Colour");
INSERT INTO tag_modifiers (name, description) VALUES ("cyan", "Colour");
INSERT INTO tag_modifiers (name, description) VALUES ("neon", "Colour");
INSERT INTO tag_modifiers (name, description) VALUES ("golden", "Colour");
INSERT INTO tag_modifiers (name, description) VALUES ("multicoloured", "Colour");


INSERT INTO tag_modifiers (name, description) VALUES ("translucent", "Colour");
INSERT INTO tag_modifiers (name, description) VALUES ("glowing", "Colour");

-- States of being
INSERT INTO tag_modifiers (name, description) VALUES ("new", "State");
INSERT INTO tag_modifiers (name, description) VALUES ("old", "State");
INSERT INTO tag_modifiers (name, description) VALUES ("curly", "State");
INSERT INTO tag_modifiers (name, description) VALUES ("straight", "State");
INSERT INTO tag_modifiers (name, description) VALUES ("dry", "State");
INSERT INTO tag_modifiers (name, description) VALUES ("wet", "State");
INSERT INTO tag_modifiers (name, description) VALUES ("local", "State");
INSERT INTO tag_modifiers (name, description) VALUES ("free", "State");
INSERT INTO tag_modifiers (name, description) VALUES ("clean", "State");
INSERT INTO tag_modifiers (name, description) VALUES ("messy", "State");
INSERT INTO tag_modifiers (name, description) VALUES ("wrong", "State");
INSERT INTO tag_modifiers (name, description) VALUES ("accurate", "State");
INSERT INTO tag_modifiers (name, description) VALUES ("dangling", "State");
INSERT INTO tag_modifiers (name, description) VALUES ("dirty", "State");
INSERT INTO tag_modifiers (name, description) VALUES ("natural", "State");
INSERT INTO tag_modifiers (name, description) VALUES ("traditional", "State");


-- Common action --
INSERT INTO tag_modifiers (name, description) VALUES ("holding", "Action");
INSERT INTO tag_modifiers (name, description) VALUES ("grabbing", "Action");

-- Widths / lenghts
INSERT INTO tag_modifiers (name, description) VALUES ("long", "Measurement");
INSERT INTO tag_modifiers (name, description) VALUES ("tall", "Measurement");
INSERT INTO tag_modifiers (name, description) VALUES ("short", "Measurement");
INSERT INTO tag_modifiers (name, description) VALUES ("large", "Measurement");

-- Aliases for modifiers --
INSERT INTO tag_modifier_aliases (name, meant_modifier) VALUES ("big", (SELECT id FROM tag_modifiers WHERE NAME="large"));
INSERT INTO tag_modifier_aliases (name, meant_modifier) VALUES ("multicolored", (SELECT id FROM tag_modifiers WHERE NAME="multicoloured"));
   

----------- Hair Tags -----------

-- Tag 'hair' with 1 alias(es)
INSERT INTO tags (name, description, category, is_private) VALUES ("hair", "Threadlike strands on the heads humans and many animals, not to be confused with fur. ", 0, 0);
INSERT INTO tag_aliases (name, meant_tag) VALUES ("dyed hair", (SELECT id FROM tags WHERE name = "hair"));

-- Tag 'flaming hair' with 1 alias(es)
INSERT INTO tags (name, description, category, is_private) VALUES ("flaming hair", "When a character has hair or a mane that is made of fire.", 0, 0);
INSERT INTO tag_aliases (name, meant_tag) VALUES ("fire hair", (SELECT id FROM tags WHERE name = "flaming hair"));
-- Implied tag 'flaming hair' => 'hair' 
INSERT INTO tag_implies (primary_tag, to_apply) VALUES ((SELECT id FROM tags WHERE name = "flaming hair"), (SELECT id FROM tags WHERE name = "hair"));

-- Tag 'hair bun' with 1 alias(es)
INSERT INTO tags (name, description, category, is_private) VALUES ("hair bun", "", 0, 0);
INSERT INTO tag_aliases (name, meant_tag) VALUES ("hairbun", (SELECT id FROM tags WHERE name = "hair bun"));
-- Implied tag 'hair bun' => 'hair' 
INSERT INTO tag_implies (primary_tag, to_apply) VALUES ((SELECT id FROM tags WHERE name = "hair bun"), (SELECT id FROM tags WHERE name = "hair"));

-- Tag 'braided hair' with 3 alias(es)
INSERT INTO tags (name, description, category, is_private) VALUES ("braided hair", "This tag is used when all or part of a characters hair is woven into plaits.", 0, 0);
INSERT INTO tag_aliases (name, meant_tag) VALUES ("plait", (SELECT id FROM tags WHERE name = "braided hair"));
INSERT INTO tag_aliases (name, meant_tag) VALUES ("braids", (SELECT id FROM tags WHERE name = "braided hair"));
INSERT INTO tag_aliases (name, meant_tag) VALUES ("braid", (SELECT id FROM tags WHERE name = "braided hair"));
-- Implied tag 'braided hair' => 'hair' 
INSERT INTO tag_implies (primary_tag, to_apply) VALUES ((SELECT id FROM tags WHERE name = "braided hair"), (SELECT id FROM tags WHERE name = "hair"));
-- Implied tag 'braided tail' => 'braided hair' 
-- Implied tag 'side braid' => 'braided hair' 
-- Implied tag 'single braid' => 'braided hair' 
-- Implied tag 'twin braids' => 'braided hair' 

-- Tag 'braided tail' 
INSERT INTO tags (name, description, category, is_private) VALUES ("braided tail", "", 0, 0);
-- Implied tag 'braided tail' => 'braided hair' 
INSERT INTO tag_implies (primary_tag, to_apply) VALUES ((SELECT id FROM tags WHERE name = "braided tail"), (SELECT id FROM tags WHERE name = "braided hair"));

-- Tag 'side braid' 
INSERT INTO tags (name, description, category, is_private) VALUES ("side braid", "", 0, 0);
-- Implied tag 'side braid' => 'braided hair' 
INSERT INTO tag_implies (primary_tag, to_apply) VALUES ((SELECT id FROM tags WHERE name = "side braid"), (SELECT id FROM tags WHERE name = "braided hair"));

-- Tag 'single braid' 
INSERT INTO tags (name, description, category, is_private) VALUES ("single braid", "This tag is used when a characters hair is woven into one single plait.", 0, 0);
-- Implied tag 'single braid' => 'braided hair' 
INSERT INTO tag_implies (primary_tag, to_apply) VALUES ((SELECT id FROM tags WHERE name = "single braid"), (SELECT id FROM tags WHERE name = "braided hair"));

-- Tag 'twin braids' with 1 alias(es)
INSERT INTO tags (name, description, category, is_private) VALUES ("twin braids", "", 0, 0);
INSERT INTO tag_aliases (name, meant_tag) VALUES ("twin braid", (SELECT id FROM tags WHERE name = "twin braids"));
-- Implied tag 'twin braids' => 'braided hair' 
INSERT INTO tag_implies (primary_tag, to_apply) VALUES ((SELECT id FROM tags WHERE name = "twin braids"), (SELECT id FROM tags WHERE name = "braided hair"));

-- Tag 'straight hair' 
INSERT INTO tags (name, description, category, is_private) VALUES ("straight hair", "", 0, 0);
-- Implied tag 'straight hair' => 'hair' 
INSERT INTO tag_implies (primary_tag, to_apply) VALUES ((SELECT id FROM tags WHERE name = "straight hair"), (SELECT id FROM tags WHERE name = "hair"));

-- Tag 'rainbow hair' with 1 alias(es)
INSERT INTO tags (name, description, category, is_private) VALUES ("rainbow hair", "Any image or animation in which a character has rainbow-colored hair.", 0, 0);
INSERT INTO tag_aliases (name, meant_tag) VALUES ("long rainbow hair", (SELECT id FROM tags WHERE name = "rainbow hair"));


-- Composite tag breaking

-- Grabbing
INSERT INTO common_composite_tags (tag_string, actual_tag) VALUES ("*grab", 0);
INSERT INTO composite_tag_modifiers (composite, modifier) VALUES ((SELECT last_insert_rowid()), (SELECT id FROM tag_modifiers WHERE name="grabbing"));


-- Tag breaking by parsing alternate string (this is like super aliases)
-- Redhead
INSERT INTO tag_super_aliases (alias, expanded) VALUES ("redhead", "red hair");

-- Brunette
INSERT INTO tag_super_aliases (alias, expanded) VALUES ("brunette", "brown hair");

-- Blonde
INSERT INTO tag_super_aliases (alias, expanded) VALUES ("blonde", "blonde hair");


END TRANSACTION;
