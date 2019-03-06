#include "catch.hpp"

#include "Common.h"
#include "DummyLog.h"
#include "TestDualView.h"

#include "TestDatabase.h"
#include "resources/Collection.h"
#include "resources/Folder.h"
#include "resources/Image.h"

#include "Common/StringOperations.h"

#include <boost/filesystem.hpp>
#include <sqlite3.h>

#include <memory>
#include <thread>

#include <iostream>

using namespace DV;

TEST_CASE("String length counting", "[db]")
{
    const char str[] = "SELECT COUNT(*) FROM tags;";

    REQUIRE(sizeof(str) == strlen(str) + 1);
}

TEST_CASE("Tag cutting", "[db][tags]")
{
    SECTION("Two parts")
    {
        std::vector<std::string> tagparts;
        Leviathan::StringOperations::CutString<std::string>("two;tags", ";", tagparts);

        REQUIRE(tagparts.size() == 2);
        CHECK(tagparts[0] == "two");
        CHECK(tagparts[1] == "tags");
    }

    SECTION("Single part")
    {
        std::vector<std::string> tagparts;
        Leviathan::StringOperations::CutString<std::string>("a tag;", ";", tagparts);

        REQUIRE(tagparts.size() == 1);
        CHECK(tagparts[0] == "a tag");
    }

    SECTION("Single part with no ;")
    {
        std::vector<std::string> tagparts;
        Leviathan::StringOperations::CutString<std::string>("tag", ";", tagparts);

        REQUIRE(tagparts.size() == 1);
        CHECK(tagparts[0] == "tag");
    }
}

int SqliteHelperCallback(void* user, int columns, char** columnsastext, char** columnname)
{
    REQUIRE(columns > 0);

    *reinterpret_cast<std::string*>(user) = std::string(columnsastext[0]);

    // Continue
    return 0;
}

TEST_CASE("Sqlite basic thing works", "[db]")
{
    sqlite3* db = nullptr;

    const auto result = sqlite3_open(":memory:", &db);

    REQUIRE(db);

    CHECK(result == SQLITE_OK);

    // Test foreign keys
    char* error = nullptr;
    REQUIRE(SQLITE_OK == sqlite3_exec(db,
                             "PRAGMA foreign_keys = ON; PRAGMA recursive_triggers = ON",
                             nullptr, nullptr, &error));

    if(error) {

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


TEST_CASE("Database in memory creation", "[db]")
{
    REQUIRE_NOTHROW(Database(true));
}

TEST_CASE("Disk database can be opened", "[db][expensive]")
{
    SECTION("Without ./")
    {
        REQUIRE_NOTHROW(Database("test_db.sqlite"));
    }

    SECTION("With ./")
    {
        REQUIRE_NOTHROW(Database("./test_db.sqlite"));
    }
}

TEST_CASE("Basic database retrieves don't throw", "[db]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    CHECK_NOTHROW(db.SelectImageByHashAG("ladlsafh"));
}

TEST_CASE("Normal database setup works", "[db][expensive]")
{
    DummyDualView dv;
    boost::filesystem::remove("test_init.sqlite");
    Database db("test_init.sqlite");

    REQUIRE_NOTHROW(db.Init());

    // There should be stuff in it //
    CHECK(db.CountExistingTags() > 0);
}

TEST_CASE("In memory initialization works and version is set", "[db]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    int version = 0;
    GUARD_LOCK_OTHER(db);

    REQUIRE(db.SelectDatabaseVersion(guard, version));

    CHECK(version > 0);
}

TEST_CASE("Directly using database for collection and image inserts", "[db]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    SECTION("Collection creation")
    {
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

    SECTION("Image creation")
    {
        auto image = db.InsertTestImage("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg",
            "II+O7pSQgH8BG_gWrc+bAetVgxJNrJNX4zhA4oWV+V0=");

        REQUIRE(image);

        // Duplicate hash
        CHECK_THROWS(
            db.InsertTestImage("second.jpg", "II+O7pSQgH8BG_gWrc+bAetVgxJNrJNX4zhA4oWV+V0="));
    }

    SECTION("Adding image to collection")
    {
        auto collection = db.InsertCollectionAG("collection for image", false);
        REQUIRE(collection);

        auto image = db.InsertTestImage("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg",
            "II+O7pSQgH8BG_gWrc+bAetVgxJNrJNX4zhA4oWV+V0=");

        REQUIRE(image);

        CHECK(db.InsertImageToCollectionAG(*collection, *image, 1));

        CHECK(collection->GetImageCount() == 1);
        CHECK(collection->GetLastShowOrder() == 1);
        CHECK(collection->GetImageShowOrder(image) == 1);

        auto image2 = db.InsertTestImage("img2.jpg", "II+actualhashwouldbehere");

        REQUIRE(image2);

        CHECK(db.InsertImageToCollectionAG(
            *collection, *image2, collection->GetLastShowOrder() + 1));

        CHECK(collection->GetImageCount() == 2);
        CHECK(collection->GetLastShowOrder() == 2);
        CHECK(collection->GetImageShowOrder(image) == 1);
        CHECK(collection->GetImageShowOrder(image2) == 2);
    }

    SECTION("Trying to add multiples")
    {
        auto collection = db.InsertCollectionAG("collection for image", false);
        REQUIRE(collection);

        auto image = db.InsertTestImage("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg",
            "II+O7pSQgH8BG_gWrc+bAetVgxJNrJNX4zhA4oWV+V0=");
        auto image2 = image;

        REQUIRE(image);

        GUARD_LOCK_OTHER(db);

        CHECK(collection->AddImage(image, guard));

        CHECK(collection->GetImageCount(guard) == 1);

        CHECK(!collection->AddImage(image2, guard));
        CHECK(collection->GetImageCount(guard) == 1);

        auto image3 = db.InsertTestImage(guard, "img2.jpg", "II++bAetVgxJNrJNX4zhA4oWV+V0=");

        REQUIRE(image3);

        CHECK(collection->AddImage(image3, 5, guard));
        CHECK(collection->GetImageCount(guard) == 2);

        auto image4 = db.InsertTestImage(guard, "randomstuff.jpg", "randomstuff");

        REQUIRE(image4);

        CHECK(collection->GetLastShowOrder(guard) == 5);
        CHECK(collection->GetImageShowOrder(image, guard) == 1);
        CHECK(collection->GetImageShowOrder(image2, guard) == 1);
        CHECK(collection->GetImageShowOrder(image3, guard) == 5);
        CHECK(collection->GetImageShowOrder(image4, guard) == -1);
    }

    SECTION("Image show order")
    {
        SECTION("Image index and ImageListScroll support")
        {
            auto collection = db.InsertCollectionAG("test collection", false);
            REQUIRE(collection);

            auto image1 = db.InsertTestImage("data/7c2c2141cf27cb90620f80400c6bc3c4.jpg",
                "II+O7pSQgH8BG_gWrc+bAetVgxJNrJNX4zhA4oWV+V0=");

            auto image2 = db.InsertTestImage("img2.jpg", "II++bAetVgxJNrJNX4zhA4oWV+V0=");

            auto image3 = db.InsertTestImage("randomstuff.jpg", "randomstuff");

            REQUIRE(image1);
            REQUIRE(image2);
            REQUIRE(image3);

            CHECK(collection->AddImage(image1));
            CHECK(collection->AddImage(image2));
            CHECK(collection->AddImage(image3));

            CHECK(collection->GetImageCount() == 3);
            CHECK(collection->GetLastShowOrder() == 3);

            CHECK(db.SelectImageShowIndexInCollection(*collection, *image1) == 0);
            CHECK(db.SelectImageShowIndexInCollection(*collection, *image2) == 1);
            CHECK(db.SelectImageShowIndexInCollection(*collection, *image3) == 2);

            CHECK(!db.SelectImageInCollectionByShowOrderAG(*collection, 0));
            CHECK(*db.SelectImageInCollectionByShowOrderAG(*collection, 1) == *image1);
            CHECK(*db.SelectImageInCollectionByShowOrderAG(*collection, 2) == *image2);
            CHECK(*db.SelectImageInCollectionByShowOrderAG(*collection, 3) == *image3);

            CHECK(*db.SelectFirstImageInCollectionAG(*collection) == *image1);
            CHECK(*db.SelectLastImageInCollectionAG(*collection) == *image3);


            CHECK(*db.SelectNextImageInCollectionByShowOrder(*collection, 0) == *image1);
            CHECK(*db.SelectNextImageInCollectionByShowOrder(*collection, 1) == *image2);
            CHECK(*db.SelectNextImageInCollectionByShowOrder(*collection, 2) == *image3);
            CHECK(!db.SelectNextImageInCollectionByShowOrder(*collection, 3));


            CHECK(!db.SelectPreviousImageInCollectionByShowOrder(*collection, 0));
            CHECK(!db.SelectPreviousImageInCollectionByShowOrder(*collection, 1));
            CHECK(*db.SelectPreviousImageInCollectionByShowOrder(*collection, 2) == *image1);
            CHECK(*db.SelectPreviousImageInCollectionByShowOrder(*collection, 3) == *image2);
            CHECK(*db.SelectPreviousImageInCollectionByShowOrder(*collection, 4) == *image3);
        }
    }
}


TEST_CASE("Directly using database for folder contents", "[db]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    SECTION("Default collections are in root folder")
    {
        const auto count = db.SelectCollectionsInFolder(*db.SelectRootFolderAG(), "").size();

        CHECK(count >= 2);

        SECTION("Filtering by name works")
        {
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

    SECTION("Create folder and add a collection to it")
    {
        auto folder = db.InsertFolder("folder1", false, *db.SelectRootFolderAG());

        REQUIRE(folder);

        auto backgrounds = db.SelectCollectionByNameAG("Backgrounds");

        REQUIRE(backgrounds);

        const auto originalSize =
            db.SelectCollectionsInFolder(*db.SelectRootFolderAG(), "").size();

        db.InsertCollectionToFolderAG(*folder, *backgrounds);

        CHECK(db.SelectCollectionsInFolder(*folder, "").size() == 1);

        CHECK(db.SelectCollectionsInFolder(*folder, "a").size() == 1);
        CHECK(db.SelectCollectionsInFolder(*folder, "aa").empty());

        SECTION("Collection stays in original folder")
        {
            CHECK(db.SelectCollectionsInFolder(*db.SelectRootFolderAG(), "").size() ==
                  originalSize);
        }

        SECTION("Removing from root folder works")
        {
            db.DeleteCollectionFromRootIfInAnotherFolder(*backgrounds);

            CHECK(db.SelectCollectionsInFolder(*db.SelectRootFolderAG(), "").size() <
                  originalSize);
        }
    }

    SECTION("Selecting subfolders")
    {
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

TEST_CASE("Directly using database for tag creating", "[db][tags]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    SECTION("Creating a simple tag")
    {
        auto tag = db.InsertTag("test tag", "tag for testing", TAG_CATEGORY::META, false);

        REQUIRE(tag);

        SECTION("Selected by name tag equals the created tag")
        {
            auto tag2 = db.SelectTagByNameAG("test tag");

            CHECK(tag2.get() == tag.get());
            CHECK(*tag == *tag2);
        }

        SECTION("Inserting duplicate causes an exception")
        {
            CHECK_THROWS(db.InsertTag(
                "test tag", "some cool tag", TAG_CATEGORY::DESCRIBE_CHARACTER_OBJECT, false));
        }
    }

    SECTION("Inserting multiple tags in a row")
    {
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

    SECTION("Tag with alias")
    {
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


    SECTION("Tag with imply")
    {
        auto tag = db.InsertTag("test tag", "tag for testing", TAG_CATEGORY::META, false);

        REQUIRE(tag);

        tag->AddImpliedTag(db.SelectTagByNameAG("captions"));

        auto implied = tag->GetImpliedTags();

        REQUIRE(implied.size() == 1);

        CHECK(implied[0]->GetName() == "captions");
    }
}

TEST_CASE("Tag parsing", "[db][tags]")
{
    std::unique_ptr<Database> dbptr(new TestDatabase());
    DummyDualView dv(std::move(dbptr));
    auto& db = dv.GetDatabase();

    REQUIRE_NOTHROW(db.Init());

    SECTION("Basic tag")
    {
        auto tag = dv.ParseTagFromString("watermark");

        CHECK(tag);
    }

    SECTION("Ending 's' is ignored correctly")
    {
        auto tag = dv.ParseTagFromString("watermarks");

        CHECK(tag);
    }

    SECTION("Spaces are removed from a single tag")
    {
        auto tag = dv.ParseTagFromString("water mark");

        CHECK(tag);
    }

    SECTION("Basic modifiers")
    {
        REQUIRE(db.SelectTagModifierByNameAG("large"));

        SECTION("Calling modifiers directly")
        {

            auto tagmods = dv.ParseTagWithOnlyModifiers("large watermark");

            REQUIRE(tagmods);

            CHECK(tagmods->GetModifiers().size() == 1);

            CHECK(tagmods->GetTagName() == "watermark");
        }

        SECTION("Letting ParseTag call it for us")
        {

            auto tag = dv.ParseTagFromString("large watermark");

            CHECK(tag);
        }
    }

    SECTION("A bunch of modifiers")
    {
        auto tagmods = dv.ParseTagWithOnlyModifiers("large tall cyan watermark");

        REQUIRE(tagmods);

        CHECK(tagmods->GetModifiers().size() == 3);

        CHECK(tagmods->GetTagName() == "watermark");

        CHECK(tagmods->GetModifiers()[0]->GetName() == "large");
        CHECK(tagmods->GetModifiers()[1]->GetName() == "tall");
        CHECK(tagmods->GetModifiers()[2]->GetName() == "cyan");

        SECTION("Modifiers in different order is the same tag")
        {

            auto tagmods2 = dv.ParseTagWithOnlyModifiers("large cyan tall watermark");

            REQUIRE(tagmods2);

            CHECK(tagmods->IsSame(*tagmods2));
        }
    }

    SECTION("Combines")
    {
        SECTION("Direct, Easy test")
        {

            auto parsed = dv.ParseTagWithComposite("captions in watermark");

            REQUIRE(std::get<0>(parsed));
            REQUIRE(std::get<1>(parsed) == "in");
            REQUIRE(std::get<2>(parsed));
        }

        SECTION("With just tags")
        {

            auto tag = dv.ParseTagFromString("captions in watermark");

            REQUIRE(tag);

            std::string combinestr;
            std::shared_ptr<AppliedTag> combined;

            CHECK(tag->GetCombinedWith(combinestr, combined));

            CHECK(combinestr == "in");
            REQUIRE(combined);
            CHECK(combined->GetTagName() == "watermark");
        }

        SECTION("Tags with modifiers")
        {

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

        SECTION("Multi word tags")
        {

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

    SECTION("Modifier alias")
    {
        auto tag1 = dv.ParseTagFromString("multicolored watermark");

        REQUIRE(tag1);

        auto tag2 = dv.ParseTagFromString("big watermark");

        REQUIRE(tag2);
    }

    SECTION("Break rules")
    {
        SECTION("Rule without wildcard")
        {
            auto tag = dv.ParseTagFromString("blonde");

            REQUIRE(tag);

            CHECK(tag->GetModifiers().size() == 1);
            CHECK(tag->GetModifiers()[0]->GetName() == "blonde");
            CHECK(tag->GetTagName() == "hair");
        }

        SECTION("Wildcard rule")
        {
            SECTION("normal wildcard")
            {
                // There actually aren't any of these, so maybe insert one here and test
            }

            SECTION("wildcard first")
            {
                auto tag = dv.ParseTagFromString("hair grab");
            }
        }
    }

    SECTION("To accurate string")
    {
        CHECK(dv.ParseTagFromString("watermark")->ToAccurateString() == "watermark");

        CHECK(
            dv.ParseTagFromString("large watermark")->ToAccurateString() == "large watermark");
    }
}

TEST_CASE("TagCollection works like it should", "[db][tags]")
{
    std::unique_ptr<Database> dbptr(new TestDatabase());
    DummyDualView dv(std::move(dbptr));
    auto& db = static_cast<TestDatabase&>(dv.GetDatabase());

    REQUIRE_NOTHROW(db.Init());

    SECTION("Non-database use")
    {
        TagCollection tags;

        CHECK(!tags.HasTags());
        CHECK(!tags.HasTags());

        CHECK(tags.Add(dv.ParseTagFromString("watermark")));

        CHECK(tags.HasTags());

        CHECK(tags.GetTagCount() == 1);

        // Same tag twice is ignored //
        CHECK(!tags.Add(dv.ParseTagFromString("watermark")));

        CHECK(tags.GetTagCount() == 1);

        CHECK(tags.Add(dv.ParseTagFromString("drawn")));

        CHECK(tags.GetTagCount() == 2);

        TagCollection tags2;

        tags2.Add(tags);

        CHECK(tags2.GetTagCount() == 2);

        CHECK(tags.RemoveTag(*dv.ParseTagFromString("watermark")));

        CHECK(tags.GetTagCount() == 1);

        CHECK(!tags.RemoveText("watermark"));

        CHECK(tags.RemoveText("drawn"));

        CHECK(tags.GetTagCount() == 0);


        CHECK(tags2.GetTagCount() == 2);

        TagCollection tags3;

        tags3.Add(tags2);
        CHECK(tags3.GetTagCount() == 2);
        tags3.Add(tags2);
        CHECK(tags3.GetTagCount() == 2);

        tags.Add(dv.ParseTagFromString("watermark"));
        CHECK(tags.GetTagCount() == 1);

        tags3.Add(tags);
        CHECK(tags3.GetTagCount() == 2);

        CHECK(tags.HasTag(*dv.ParseTagFromString("watermark")));
        CHECK(tags.HasTags());
        tags.Clear();
        CHECK(!tags.HasTags());
    }

    SECTION("To string and parsing back")
    {
        TagCollection tags;

        CHECK(tags.Add(dv.ParseTagFromString("watermark")));
        CHECK(tags.Add(dv.ParseTagFromString("drawn")));

        CHECK(tags.GetTagCount() == 2);

        std::string str = tags.TagsAsString("\n");

        CHECK(str == "watermark\ndrawn");

        TagCollection tags2;

        CHECK(tags2.Add(dv.ParseTagFromString("hair")));
        CHECK(tags2.HasTag(*dv.ParseTagFromString("hair")));

        tags2.ReplaceWithText(str);

        CHECK(!tags2.HasTag(*dv.ParseTagFromString("hair")));
        CHECK(tags2.HasTag(*dv.ParseTagFromString("drawn")));

        CHECK(tags2.GetTagCount() == 2);
    }

    SECTION("With modifiers and other stuff")
    {
        TagCollection tags;

        CHECK(tags.Add(dv.ParseTagFromString("large watermark")));
        CHECK(tags.Add(dv.ParseTagFromString("silver drawn")));

        CHECK(tags.GetTagCount() == 2);

        CHECK(tags.Add(dv.ParseTagFromString("watermark")));
        CHECK(tags.GetTagCount() == 3);

        CHECK(!tags.Add(dv.ParseTagFromString("large watermark")));
        CHECK(tags.GetTagCount() == 3);

        CHECK(tags.Add(dv.ParseTagFromString("watermark in hair")));
        CHECK(tags.GetTagCount() == 4);

        CHECK(!tags.Add(dv.ParseTagFromString("watermark in hair")));
        CHECK(tags.GetTagCount() == 4);

        CHECK(tags.TagsAsString(" - ") ==
              "large watermark - silver drawn - watermark - watermark in hair");

        CHECK(tags.RemoveText("large watermark"));
        CHECK(tags.GetTagCount() == 3);

        CHECK(tags.RemoveTag(*dv.ParseTagFromString("silver drawn")));
        CHECK(tags.GetTagCount() == 2);

        CHECK(tags.Add(dv.ParseTagFromString("watermark on hair")));
        CHECK(tags.GetTagCount() == 3);

        CHECK(tags.Add(dv.ParseTagFromString("watermark in captions")));
        CHECK(tags.GetTagCount() == 4);

        CHECK(!tags.Add(dv.ParseTagFromString("watermark in captions")));
        CHECK(tags.GetTagCount() == 4);
    }

    SECTION("Manipulating image tags")
    {
        // Insert image //
        auto img = db.InsertTestImage("our image", "coolhashgoeshere");

        REQUIRE(img);
        CHECK(img.use_count() == 1);

        auto tags = img->GetTags();

        REQUIRE(tags);

        CHECK(!tags->HasTags());
        CHECK(tags->Add(dv.ParseTagFromString("watermark on hair")));
        CHECK(tags->HasTags());
        CHECK(tags->HasTag(*dv.ParseTagFromString("watermark on hair")));
        CHECK(tags->GetTagCount() == 1);

        CHECK(tags->Add(dv.ParseTagFromString("drawn")));
        CHECK(tags->GetTagCount() == 2);
        CHECK(tags->RemoveText("drawn"));
        CHECK(tags->GetTagCount() == 1);

        // Reloads the image from the database //
        Image* oldptr = img.get();
        TagCollection* oldtags = tags.get();
        CHECK(img.use_count() == 1);
        img.reset();
        tags.reset();

        img = db.SelectImageByHashAG("coolhashgoeshere");

        REQUIRE(img);

        tags = img->GetTags();

        REQUIRE(tags);

        // Looks like if nothing gets allocated before this the pointers are the same...
        // if(oldptr == img.get() && (oldtags != tags.get()))
        // CHECK(false);

        CHECK(tags->HasTags());
        CHECK(tags->GetTagCount() == 1);

        auto tag1 = *tags->begin();

        REQUIRE(tag1);

        REQUIRE(tag1->GetTag());
        CHECK(tag1->GetTag()->GetName() == "watermark");
        CHECK(tag1->ToAccurateString() == "watermark on hair");

        CHECK(tags->HasTag(*dv.ParseTagFromString("watermark on hair")));
    }

    SECTION("Directly testing SelectExistingAppliedTagID")
    {
        auto img = db.InsertTestImage("our image", "coolhashgoeshere");
        REQUIRE(img);

        auto tagtoinsert = dv.ParseTagFromString("watermark");

        {
            GUARD_LOCK_OTHER(db);

            db.InsertImageTag(guard, img, *tagtoinsert);
        }

        CHECK(db.CountAppliedTags() == 1);
        {
            GUARD_LOCK_OTHER(db);

            // Here we assume that the first id is 1
            {
                auto tag = db.SelectAppliedTagByID(guard, 1);

                REQUIRE(tag);
            }

            // Test the parts first //
            CHECK(db.CheckDoesAppliedTagModifiersMatch(guard, 1, *tagtoinsert));

            CHECK(db.CheckDoesAppliedTagCombinesMatch(guard, 1, *tagtoinsert));

            // Then the whole thing
            CHECK(db.SelectExistingAppliedTagID(guard, *tagtoinsert));
        }

        tagtoinsert = dv.ParseTagFromString("watermark");
        GUARD_LOCK_OTHER(db);
        CHECK(db.SelectExistingAppliedTagID(guard, *tagtoinsert));
    }

    SECTION("Image tags share the same ids when adding to multiple images")
    {
        auto img = db.InsertTestImage("our image", "coolhashgoeshere");
        REQUIRE(img);

        auto img2 = db.InsertTestImage("second", "coolhashgoeshere2154");
        REQUIRE(img2);

        auto tags = img->GetTags();
        REQUIRE(tags);

        auto tags2 = img2->GetTags();
        REQUIRE(tags2);


        SECTION("One simple tag per image")
        {
            CHECK(tags->Add(dv.ParseTagFromString("watermark")));
            CHECK(db.CountAppliedTags() == 1);

            CHECK(tags2->Add(dv.ParseTagFromString("watermark")));
            CHECK(db.CountAppliedTags() == 1);

            CHECK(tags2->Add(dv.ParseTagFromString("hair")));
            CHECK(db.CountAppliedTags() == 2);
        }

        SECTION("Complex tags")
        {
            // db.PrintAppliedTagTable();
        }
    }
}

TEST_CASE("Virtual folder Path parsing works", "[folder][path][db]")
{
    std::unique_ptr<Database> dbptr(new TestDatabase());
    DummyDualView dv(std::move(dbptr));
    auto& db = static_cast<TestDatabase&>(dv.GetDatabase());

    REQUIRE_NOTHROW(db.Init());

    auto root = dv.GetRootFolder();
    REQUIRE(root);

    SECTION("Root path")
    {
        auto folder = dv.GetFolderFromPath(VirtualPath("Root/"));
        REQUIRE(folder);

        CHECK(*folder == *root);
    }

    SECTION("Simple path")
    {
        // Probably should add a test for inserting folders
        auto inserted = db.InsertFolder("nice folder", false, *root);

        auto folder = dv.GetFolderFromPath(VirtualPath("Root/nice folder"));
        REQUIRE(folder);

        CHECK(*folder == *inserted);
    }

    SECTION("Long path")
    {
        auto inserted1 = db.InsertFolder("nice folder", false, *root);

        auto inserted2 = db.InsertFolder("subfolder", false, *inserted1);

        auto inserted3 = db.InsertFolder("more parts", false, *inserted2);

        auto inserted4 = db.InsertFolder("last", false, *inserted3);

        auto folder =
            dv.GetFolderFromPath(VirtualPath("Root/nice folder/subfolder/more parts/last"));
        REQUIRE(folder);

        CHECK(*folder == *inserted4);
    }
}

TEST_CASE("Reverse folder path from folder works", "[folder][path][db]")
{
    std::unique_ptr<Database> dbptr(new TestDatabase());
    DummyDualView dv(std::move(dbptr));
    auto& db = static_cast<TestDatabase&>(dv.GetDatabase());

    REQUIRE_NOTHROW(db.Init());

    auto root = dv.GetRootFolder();
    REQUIRE(root);

    SECTION("Root folder")
    {
        CHECK(dv.ResolvePathToFolder(root->GetID()).IsRootPath());
    }

    SECTION("Single depth")
    {
        auto inserted1 = db.InsertFolder("nice folder", false, *root);

        CHECK(static_cast<std::string>(dv.ResolvePathToFolder(inserted1->GetID())) ==
              "Root/nice folder");
    }

    SECTION("Deep testing")
    {
        auto inserted1 = db.InsertFolder("nice folder", false, *root);

        auto inserted2 = db.InsertFolder("subfolder", false, *inserted1);

        CHECK(static_cast<std::string>(dv.ResolvePathToFolder(inserted2->GetID())) ==
              "Root/nice folder/subfolder");
    }

    SECTION("Multiple parents")
    {
        // TODO: add support for this in the database
    }

    SECTION("Simple loop")
    {
        // TODO: this too needs support
    }
}

TEST_CASE("Specific database applied tag is same", "[tags][db]")
{
    std::unique_ptr<Database> dbptr(new TestDatabase());
    DummyDualView dv(std::move(dbptr));
    auto& db = static_cast<TestDatabase&>(dv.GetDatabase());

    REQUIRE_NOTHROW(db.Init());

    SECTION("brown hair == brown hair")
    {
        // 20449
        // 20458
        auto hairid = db.SelectTagByNameAG("hair");
        auto brownmod = db.SelectTagModifierByNameAG("brown");

        REQUIRE(hairid);
        REQUIRE(brownmod);

        db.Run("BEGIN TRANSACTION;");
        db.Run("INSERT INTO applied_tag (id, tag) VALUES (?, ?)", 20449, hairid->GetID());
        db.Run("INSERT INTO applied_tag_modifier (to_tag, modifier) VALUES (?, ?)", 20449,
            brownmod->GetID());

        db.Run("INSERT INTO applied_tag (id, tag) VALUES (?, ?)", 20458, hairid->GetID());
        db.Run("INSERT INTO applied_tag_modifier (to_tag, modifier) VALUES (?, ?)", 20458,
            brownmod->GetID());

        db.Run("COMMIT TRANSACTION;");

        auto tag1 = db.SelectAppliedTagByIDAG(20449);

        REQUIRE(tag1);
        REQUIRE(tag1->ToAccurateString() == "brown hair");

        auto tag2 = db.SelectAppliedTagByIDAG(20458);

        REQUIRE(tag2);
        REQUIRE(tag2->ToAccurateString() == "brown hair");

        GUARD_LOCK_OTHER(db);

        CHECK(db.CheckDoesAppliedTagModifiersMatch(guard, 20458, *tag1));
        CHECK(db.CheckDoesAppliedTagCombinesMatch(guard, 20458, *tag1));

        CHECK(db.CheckDoesAppliedTagModifiersMatch(guard, 20449, *tag2));
        CHECK(db.CheckDoesAppliedTagCombinesMatch(guard, 20449, *tag2));
    }
}


TEST_CASE("Tag suggestions", "[db][tags]")
{
    std::unique_ptr<Database> dbptr(new TestDatabase());
    DummyDualView dv(std::move(dbptr));
    auto& db = dv.GetDatabase();

    REQUIRE_NOTHROW(db.Init());

    SECTION("Single tag completion")
    {
        auto suggestions = dv.GetSuggestionsForTag("wat");
        CHECK(suggestions.size() > 0);

        CHECK(std::find(suggestions.begin(), suggestions.end(), "watermark") !=
              suggestions.end());
    }

    SECTION("Tag with modifiers completion")
    {
        auto tag = dv.ParseTagFromString("large watermark");
        REQUIRE(tag);

        auto suggestions = dv.GetSuggestionsForTag("large wate");
        CHECK(suggestions.size() > 0);

        CHECK(std::find(suggestions.begin(), suggestions.end(), "large watermark") !=
              suggestions.end());
    }

    SECTION("Modifier completion")
    {
        auto suggestions = dv.GetSuggestionsForTag("lar");
        CHECK(suggestions.size() > 0);

        CHECK(std::find(suggestions.begin(), suggestions.end(), "large") != suggestions.end());
    }

    SECTION("Random tag1")
    {
        auto suggestions = dv.GetSuggestionsForTag("pink wa");
        CHECK(suggestions.size() > 0);

        CHECK(std::find(suggestions.begin(), suggestions.end(), "pink watermark") !=
              suggestions.end());
    }

    SECTION("Random tag2")
    {
        auto suggestions = dv.GetSuggestionsForTag("pink ev");
        CHECK(suggestions.size() > 0);

        CHECK(std::find(suggestions.begin(), suggestions.end(), "pink eve online") !=
              suggestions.end());
    }

    SECTION("Combines with beginning")
    {
        auto suggestions = dv.GetSuggestionsForTag("captions in w");
        CHECK(suggestions.size() > 0);

        CHECK(std::find(suggestions.begin(), suggestions.end(), "captions in watermark") !=
              suggestions.end());
    }

    SECTION("Combines with modifiers")
    {
        auto suggestions = dv.GetSuggestionsForTag("white captions in black w");
        CHECK(suggestions.size() > 0);

        CHECK(std::find(suggestions.begin(), suggestions.end(),
                  "white captions in black watermark") != suggestions.end());
    }

    // When there are tags like "humanoid figure" and "figure head" completion shouldn't
    // include "humanoid figure head"
    SECTION("Multi word tag doesn't complete extra stuff")
    {
        db.InsertTag("humanoid figure", "", TAG_CATEGORY::DESCRIBE_CHARACTER_OBJECT, false);
        db.InsertTag("figure head", "", TAG_CATEGORY::DESCRIBE_CHARACTER_OBJECT, false);
        db.InsertTag("head officer", "", TAG_CATEGORY::DESCRIBE_CHARACTER_OBJECT, false);

        SECTION("first word")
        {
            auto suggestions = dv.GetSuggestionsForTag("humano");

            CHECK(std::find(suggestions.begin(), suggestions.end(), "humanoid figure") !=
                  suggestions.end());

            CHECK(std::find(suggestions.begin(), suggestions.end(), "figure head") ==
                  suggestions.end());

            CHECK(std::find(suggestions.begin(), suggestions.end(), "head officer") ==
                  suggestions.end());
        }

        SECTION("Second word figure")
        {
            auto suggestions = dv.GetSuggestionsForTag("humanoid fig");

            CHECK(std::find(suggestions.begin(), suggestions.end(), "humanoid figure") !=
                  suggestions.end());

            CHECK(std::find(suggestions.begin(), suggestions.end(), "figure head") ==
                  suggestions.end());

            CHECK(std::find(suggestions.begin(), suggestions.end(), "head officer") ==
                  suggestions.end());

            // This is most likely what is wrong
            CHECK(std::find(suggestions.begin(), suggestions.end(), "humanoid figure head") ==
                  suggestions.end());
        }

        SECTION("Second word head")
        {
            auto suggestions = dv.GetSuggestionsForTag("figure hea");

            CHECK(std::find(suggestions.begin(), suggestions.end(), "humanoid figure") ==
                  suggestions.end());

            CHECK(std::find(suggestions.begin(), suggestions.end(), "figure head") !=
                  suggestions.end());

            CHECK(std::find(suggestions.begin(), suggestions.end(), "head officer") ==
                  suggestions.end());

            // This is most likely what is wrong
            CHECK(std::find(suggestions.begin(), suggestions.end(), "figure head officer") ==
                  suggestions.end());
        }
    }
}
