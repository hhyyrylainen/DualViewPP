#include "catch.hpp"

#include "TestDualView.h"
#include "DummyLog.h"
#include "Common.h"

#include "TestDatabase.h"
#include "core/resources/Collection.h"

#include <sqlite3.h>
#include <boost/filesystem.hpp>

#include <thread>
#include <memory>

#include <iostream>

using namespace DV;

TEST_CASE("String length counting", "[db]"){

    const char str[] = "SELECT COUNT(*) FROM tags;";

    REQUIRE(sizeof(str) == strlen(str) + 1);
}

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

    SECTION("Without ./"){
        boost::filesystem::remove("test_db.sqlite");
    
        REQUIRE_NOTHROW(Database("test_db.sqlite"));
    }

    SECTION("With ./"){

        boost::filesystem::remove("test_db.sqlite");
    
        REQUIRE_NOTHROW(Database("./test_db.sqlite"));
    }
}

TEST_CASE("Basic database retrieves don't throw", "[db]"){

    DummyDualView dv;
    Database db(true);

    REQUIRE_NOTHROW(db.Init());

    CHECK_NOTHROW(db.SelectImageByHashAG("ladlsafh"));
}

TEST_CASE("Normal database setup works", "[db][expensive]"){

    DummyDualView dv;
    boost::filesystem::remove("test_init.sqlite");
    Database db("test_init.sqlite");

    REQUIRE_NOTHROW(db.Init());

    // There should be stuff in it //
    CHECK(db.CountExistingTags() > 0);
}

TEST_CASE("Directly using database for collection and image inserts", "[db]"){

    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    SECTION("Collection creation"){

        auto collection = db.InsertCollectionAG("test collection", false);
        REQUIRE(collection);
        CHECK(collection->GetName() == "test collection");

        // Same object returned
        CHECK(collection.get() == db.SelectCollectionByNameAG("test collection").get());

        // A new collection
        auto collection2 = db.InsertCollectionAG("cool stuff", false);
        REQUIRE(collection2);
        CHECK(collection2->GetName() == "cool stuff");

        // Same object returned
        CHECK(collection2.get() == db.SelectCollectionByNameAG("cool stuff").get());

        CHECK(collection.get() != collection2.get());
    }

    SECTION("Image creation"){

        auto image = db.InsertTestImage("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg",
            "II+O7pSQgH8BG_gWrc+bAetVgxJNrJNX4zhA4oWV+V0=");

        REQUIRE(image);

        // Duplicate hash
        CHECK_THROWS(db.InsertTestImage("second.jpg",
                "II+O7pSQgH8BG_gWrc+bAetVgxJNrJNX4zhA4oWV+V0="));
    }

    SECTION("Adding image to collection"){

        auto collection = db.InsertCollectionAG("collection for image", false);
        REQUIRE(collection);

        auto image = db.InsertTestImage("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg",
            "II+O7pSQgH8BG_gWrc+bAetVgxJNrJNX4zhA4oWV+V0=");

        REQUIRE(image);

        CHECK(db.InsertImageToCollection(*collection, *image, 1));

        CHECK(collection->GetImageCount() == 1);
        CHECK(collection->GetLastShowOrder() == 1);
        CHECK(collection->GetImageShowOrder(image) == 1);

        auto image2 = db.InsertTestImage("img2.jpg",
            "II+actualhashwouldbehere");

        REQUIRE(image2);

        CHECK(db.InsertImageToCollection(*collection, *image2,
                collection->GetLastShowOrder() + 1));

        CHECK(collection->GetImageCount() == 2);
        CHECK(collection->GetLastShowOrder() == 2);
        CHECK(collection->GetImageShowOrder(image) == 1);
        CHECK(collection->GetImageShowOrder(image2) == 2);
    }

    SECTION("Trying to add multiples"){

        auto collection = db.InsertCollectionAG("collection for image", false);
        REQUIRE(collection);

        auto image = db.InsertTestImage("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg",
            "II+O7pSQgH8BG_gWrc+bAetVgxJNrJNX4zhA4oWV+V0=");
        auto image2 = image;

        REQUIRE(image);

        CHECK(collection->AddImage(image));

        CHECK(collection->GetImageCount() == 1);

        CHECK(!collection->AddImage(image2));
        CHECK(collection->GetImageCount() == 1);

        auto image3 = db.InsertTestImage("img2.jpg",
            "II++bAetVgxJNrJNX4zhA4oWV+V0=");

        REQUIRE(image3);

        CHECK(collection->AddImage(image3, 5));
        CHECK(collection->GetImageCount() == 2);

        auto image4 = db.InsertTestImage("randomstuff.jpg",
            "randomstuff");

        REQUIRE(image4);

        CHECK(collection->GetLastShowOrder() == 5);
        CHECK(collection->GetImageShowOrder(image) == 1);
        CHECK(collection->GetImageShowOrder(image2) == 1);
        CHECK(collection->GetImageShowOrder(image3) == 5);
        CHECK(collection->GetImageShowOrder(image4) == -1);
    }
}


TEST_CASE("Directly using database for folder contents", "[db]"){

    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    SECTION("Default collections are in root folder"){

        const auto count = db.SelectCollectionsInFolder(*db.SelectRootFolderAG(), "").size();

        CHECK(count >= 2);

        SECTION("Filtering by name works"){
            CHECK(db.SelectCollectionsInFolder(*db.SelectRootFolderAG(), "no match").empty());
        }

        SECTION("Trying to delete from root folder doesn't work if collection "
            "isn't in other folder")
        {
            auto backgrounds = db.SelectCollectionByNameAG("Backgrounds");

            REQUIRE(backgrounds);
            
            db.DeleteCollectionFromRootIfInAnotherFolder(*backgrounds);

            CHECK(db.SelectCollectionsInFolder(*db.SelectRootFolderAG(), "").size() == count);
        }
    }

    SECTION("Create folder and add a collection to it"){

        auto folder = db.InsertFolder("folder1", false, *db.SelectRootFolderAG());

        REQUIRE(folder);

        auto backgrounds = db.SelectCollectionByNameAG("Backgrounds");

        REQUIRE(backgrounds);

        const auto originalSize = db.SelectCollectionsInFolder(*db.SelectRootFolderAG(),
            "").size();

        db.InsertCollectionToFolderAG(*folder, *backgrounds);

        CHECK(db.SelectCollectionsInFolder(*folder, "").size() == 1);

        CHECK(db.SelectCollectionsInFolder(*folder, "a").size() == 1);
        CHECK(db.SelectCollectionsInFolder(*folder, "aa").empty());

        SECTION("Collection stays in original folder"){

            CHECK(db.SelectCollectionsInFolder(*db.SelectRootFolderAG(), "").size() ==
                originalSize);
        }

        SECTION("Removing from root folder works"){

            db.DeleteCollectionFromRootIfInAnotherFolder(*backgrounds);
                
            CHECK(db.SelectCollectionsInFolder(*db.SelectRootFolderAG(), "").size() <
                originalSize);
        }
    }

    SECTION("Selecting subfolders"){

        CHECK(db.SelectFoldersInFolder(*db.SelectRootFolderAG(), "").size() == 0);

        auto folder = db.InsertFolder("folder1", false, *db.SelectRootFolderAG());

        REQUIRE(folder);

        auto folder2 = db.InsertFolder("folder2", false, *db.SelectRootFolderAG());

        REQUIRE(folder);

        auto folder3 = db.InsertFolder("sub for 1", false, *folder);

        REQUIRE(folder);

        auto folder4 = db.InsertFolder("sub sub sub sub for 1", false, *folder3);

        REQUIRE(folder);
        
        CHECK(db.SelectFoldersInFolder(*db.SelectRootFolderAG(), "").size() == 2);
        
        CHECK(db.SelectFoldersInFolder(*folder, "").size() == 1);

        CHECK(db.SelectFoldersInFolder(*folder2, "").empty());

        CHECK(db.SelectFoldersInFolder(*folder3, "").size() == 1);
    }

}

