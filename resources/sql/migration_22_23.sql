-- Migration from database version 22 to 23 --
BEGIN TRANSACTION;

CREATE TABLE action_history (

   id INTEGER PRIMARY KEY AUTOINCREMENT,

   -- Type as integer from the enum DATABASE_ACTION_TYPE
   type INTEGER NOT NULL,

   -- All the action data serialized in JSON form
   json_data TEXT NOT NULL
);

COMMIT TRANSACTION;
