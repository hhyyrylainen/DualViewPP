#include "catch.hpp"

#include "TestDualView.h"
#include "DummyLog.h"
#include "Common.h"

#include "core/Database.h"

#include <sqlite3.h>
#include <boost/filesystem.hpp>

#include <thread>
#include <memory>

#include <iostream>

using namespace DV;

int SqliteHelperCallback(void* user, int columns, char** columnsastext, char** columnname){

    REQUIRE(columns > 0);

    *reinterpret_cast<std::string*>(user) = std::string(columnsastext[0]);
    
    // Continue 
    return 0;
}

TEST_CASE("Sqlite basic thing works", "[db]"){

    sqlite3* db = nullptr;

    const auto result = sqlite3_open(":memory:", &db);

    REQUIRE(db);

    CHECK(result == SQLITE_OK);

    // Test foreign keys
    char* error = nullptr;
    REQUIRE(SQLITE_OK == sqlite3_exec(db,
            "PRAGMA foreign_keys = ON; PRAGMA recursive_triggers = ON",
            nullptr, nullptr,
             &error));
    
    if(error){

        CHECK(std::string(error) == "");
        sqlite3_free(error);
        return;
    }
    
    // Verify it worked //
    std::string pragmaForeignKeys;

    REQUIRE(SQLITE_OK == sqlite3_exec(db, "PRAGMA foreign_keys;", &SqliteHelperCallback,
            &pragmaForeignKeys, &error));

    CHECK(pragmaForeignKeys == "1");


    std::string pragmaRecursiveTriggers;

    REQUIRE(SQLITE_OK == sqlite3_exec(db, "PRAGMA recursive_triggers;", &SqliteHelperCallback,
            &pragmaRecursiveTriggers, &error));

    CHECK(pragmaRecursiveTriggers == "1");

    sqlite3_close(db);
}


TEST_CASE("Database in memory creation", "[db]"){

    REQUIRE_NOTHROW(Database(true));
}

TEST_CASE("Disk database can be opened", "[db]"){

    boost::filesystem::remove("test_db.sqlite");
    
    REQUIRE_NOTHROW(Database("test_db.sqlite"));
}

TEST_CASE("Normal database setup works", "[db][expensive]"){

    boost::filesystem::remove("test_init.sqlite");
    Database db("test_init.sqlite");

    REQUIRE_NOTHROW(db.Init());

    // There should be stuff in it //
    CHECK(db.CountExistingTags() > 0);
}


