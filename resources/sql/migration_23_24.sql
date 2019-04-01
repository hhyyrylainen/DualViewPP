-- Migration from database version 23 to 24 --

CREATE TABLE action_history (

   id INTEGER PRIMARY KEY AUTOINCREMENT,

   -- Type as integer from the enum DATABASE_ACTION_TYPE
   type INTEGER NOT NULL,

   -- 1 When performed 0 otherwise
   performed INTEGER NOT NULL DEFAULT 0,

   -- All the action data serialized in JSON form
   json_data TEXT NOT NULL
);
