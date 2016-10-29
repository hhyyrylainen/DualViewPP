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

TEST_CASE("Directly using database for tag creating", "[db][tags]"){
    
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    SECTION("Creating a simple tag"){

        auto tag = db.InsertTag("test tag", "tag for testing", TAG_CATEGORY::META, false);

        REQUIRE(tag);

        SECTION("Selected by name tag equals the created tag"){

            auto tag2 = db.SelectTagByNameAG("test tag");

            CHECK(tag2.get() == tag.get());
            CHECK(*tag == *tag2);
        }

        SECTION("Inserting duplicate causes an exception"){

            CHECK_THROWS(db.InsertTag("test tag", "some cool tag",
                    TAG_CATEGORY::DESCRIBE_CHARACTER_OBJECT, false));
        }
    }

    SECTION("Inserting multiple tags in a row"){

        auto tag1 = db.InsertTag("tag1", "tag for testing", TAG_CATEGORY::META, false);
        auto tag2 = db.InsertTag("other tag", "tag for testing", TAG_CATEGORY::META, false);
        auto tag3 = db.InsertTag("more tag", "tag for testing", TAG_CATEGORY::META, false);
        auto tag4 = db.InsertTag("tag4", "tag for testing", TAG_CATEGORY::META, true);
        auto tag5 = db.InsertTag("tag5", "tag for testing", TAG_CATEGORY::CHARACTER, false);

        REQUIRE(tag1);
        REQUIRE(tag2);
        REQUIRE(tag3);
        REQUIRE(tag4);
        REQUIRE(tag5);
    }

    SECTION("Tag with alias"){

        CHECK(!db.SelectTagByAliasAG("test"));

        auto tag = db.InsertTag("test tag", "tag for testing", TAG_CATEGORY::META, false);

        REQUIRE(tag);

        tag->AddAlias("test");

        auto tag2 = db.SelectTagByAliasAG("test");

        CHECK(tag2);

        CHECK(tag == tag2);

        tag->RemoveAlias("test");

        CHECK(!db.SelectTagByAliasAG("test"));
    }
    
}

TEST_CASE("Tag parsing", "[db][tags]"){

    std::unique_ptr<Database> dbptr(new TestDatabase());
    DummyDualView dv(std::move(dbptr));
    auto& db = dv.GetDatabase();

    REQUIRE_NOTHROW(db.Init());

    SECTION("Basic tag"){
        
        auto tag = dv.ParseTagFromString("watermark");
        
        CHECK(tag);
    }

    SECTION("Ending 's' is ignored correctly"){

        auto tag = dv.ParseTagFromString("watermarks");
        
        CHECK(tag);
    }

    SECTION("Spaces are removed from a single tag"){

        auto tag = dv.ParseTagFromString("water mark");
        
        CHECK(tag);
    }

    SECTION("Basic modifiers"){

        REQUIRE(db.SelectTagModifierByNameAG("large"));

        SECTION("Calling modifiers directly"){

            auto tagmods = dv.ParseTagWithOnlyModifiers("large watermark");

            REQUIRE(tagmods);
            
            CHECK(tagmods->GetModifiers().size() == 1);

            CHECK(tagmods->GetTagName() == "watermark");
        }

        SECTION("Letting ParseTag call it for us"){
            
            auto tag = dv.ParseTagFromString("large watermark");

            CHECK(tag);
        }
    }

    SECTION("A bunch of modifiers"){

        auto tagmods = dv.ParseTagWithOnlyModifiers("large tall cyan watermark");

        REQUIRE(tagmods);
            
        CHECK(tagmods->GetModifiers().size() == 3);

        CHECK(tagmods->GetTagName() == "watermark");

        CHECK(tagmods->GetModifiers()[0]->GetName() == "large");
        CHECK(tagmods->GetModifiers()[1]->GetName() == "tall");
        CHECK(tagmods->GetModifiers()[2]->GetName() == "cyan");

        SECTION("Modifiers in different order is the same tag"){

            auto tagmods2 = dv.ParseTagWithOnlyModifiers("large cyan tall watermark");

            REQUIRE(tagmods2);

            CHECK(tagmods->IsSame(*tagmods2));
        }
    }

    SECTION("Combines"){

        SECTION("Direct, Easy test"){

            auto parsed = dv.ParseTagWithComposite("captions in watermark");

            REQUIRE(std::get<0>(parsed));
            REQUIRE(std::get<1>(parsed) == "in");
            REQUIRE(std::get<2>(parsed));
        }

        SECTION("With just tags"){

            auto tag = dv.ParseTagFromString("captions in watermark");

            REQUIRE(tag);

            std::string combinestr;
            std::shared_ptr<AppliedTag> combined;

            CHECK(tag->GetCombinedWith(combinestr, combined));

            CHECK(combinestr == "in");
            REQUIRE(combined);
            CHECK(combined->GetTagName() == "watermark");
        }

        SECTION("Tags with modifiers"){

            auto tag = dv.ParseTagFromString("long captions in tall watermark");

            REQUIRE(tag);

            std::string combinestr;
            std::shared_ptr<AppliedTag> combined;

            CHECK(tag->GetCombinedWith(combinestr, combined));

            CHECK(combinestr == "in");
            REQUIRE(combined);
            CHECK(combined->GetTagName() == "watermark");

            REQUIRE(tag->GetModifiers().size() == 1);
            REQUIRE(tag->GetModifiers()[0]->GetName() == "long");

            REQUIRE(combined->GetModifiers().size() == 1);
            REQUIRE(combined->GetModifiers()[0]->GetName() == "tall");
        }

        SECTION("Multi word tags"){

            auto tag = dv.ParseTagFromString("eve online vs star wars");

            REQUIRE(tag);

            std::string combinestr;
            std::shared_ptr<AppliedTag> combined;

            CHECK(tag->GetCombinedWith(combinestr, combined));
            
            CHECK(combinestr == "vs");
            REQUIRE(combined);
            CHECK(combined->GetTagName() == "star wars");
            CHECK(tag->GetTagName() == "eve online");
        }
    }

    SECTION("Modifier alias"){

        auto tag1 = dv.ParseTagFromString("multicolored watermark");

        REQUIRE(tag1);

        auto tag2 = dv.ParseTagFromString("big watermark");

        REQUIRE(tag2);
    }

    SECTION("Break rules"){

        SECTION("Rule without wildcard"){

            auto tag = dv.ParseTagFromString("blonde");

            REQUIRE(tag);

            CHECK(tag->GetModifiers().size() == 1);
            CHECK(tag->GetModifiers()[0]->GetName() == "blonde");
            CHECK(tag->GetTagName() == "hair");
        }

        SECTION("Wildcard rule"){

            SECTION("normal wildcard"){

                // There actually aren't any of these, so maybe insert one here and test
            }

            SECTION("wildcard first"){

                auto tag = dv.ParseTagFromString("hair grab");
            }
        }
    }
    
}

TEST_CASE("TagCollection works like it should", "[db][tags]"){

    std::unique_ptr<Database> dbptr(new TestDatabase());
    DummyDualView dv(std::move(dbptr));
    auto& db = dv.GetDatabase();

    REQUIRE_NOTHROW(db.Init());
    
    SECTION("Non-database use"){

        TagCollection tags;

        CHECK(!tags.HasTags());
        CHECK(!tags.HasTags());

        CHECK(tags.Add(dv.ParseTagFromString("watermark")));

        CHECK(tags.HasTags());

        CHECK(tags.GetTagCount() == 1);

        // Same tag twice is ignored //
        CHECK(!tags.Add(dv.ParseTagFromString("watermark")));

        CHECK(tags.GetTagCount() == 1);
        
        
    }

    SECTION("Manipulating image tags"){


        
    }
}

