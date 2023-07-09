#include "catch.hpp"

#include "resources/Collection.h"
#include "resources/DatabaseAction.h"

#include "DummyLog.h"
#include "TestDatabase.h"
#include "TestDualView.h"

using namespace DV;

TEST_CASE("Collection name sanitization works", "[collection][file]")
{
    Leviathan::TestLogger log("test_collection.txt");

    // Sanitization fatal asserts, or returns an empty string if it fails //

    SECTION("Simple cases")
    {
        SECTION("My backgrounds")
        {
            Collection collection("My backgrounds");

            CHECK(!collection.GetNameForFolder().empty());
        }

        SECTION("My cool stuff / funny things")
        {
            Collection collection("My cool stuff / funny things");

            CHECK(!collection.GetNameForFolder().empty());
        }

        SECTION("Just this | actually more stuff: here")
        {
            Collection collection("Just this | actually more stuff: here");

            CHECK(!collection.GetNameForFolder().empty());
        }
    }

    // These are some really trick things to check
    SECTION("Corner cases")
    {
        SECTION("Dots")
        {
            {
                Collection collection(".");
                CHECK(collection.GetNameForFolder() != ".");
            }

            {
                Collection collection("..");
                CHECK(collection.GetNameForFolder() != "..");
            }
        }
    }
}

TEST_CASE("Collection rename works", "[collection][action]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    auto collection1 = db.InsertCollectionAG("Collection 1", false);
    auto collection2 = db.InsertCollectionAG("Collection 2", false);
    auto collection3 = db.InsertCollectionAG("collection 3", false);

    SECTION("Simple rename")
    {
        REQUIRE(collection1->Rename("New collection"));
        CHECK(collection1->GetName() == "New collection");
        CHECK(db.SelectCollectionByNameAG("New collection") == collection1);
    }

    SECTION("Fix capitalization in current name")
    {
        REQUIRE(collection1->Rename("collection 1"));
        CHECK(db.SelectCollectionByNameAG("collection 1") == collection1);
    }

    const auto previousName = collection1->GetName();

    SECTION("Conflict disallows rename")
    {
        SECTION("exact")
        {
            CHECK(!collection1->Rename("Collection 2"));
            CHECK(previousName == collection1->GetName());
        }

        SECTION("case difference")
        {
            CHECK(!collection1->Rename("collection 2"));
            CHECK(previousName == collection1->GetName());
        }
    }

    SECTION("Can't rename add '/'")
    {
        CHECK(!collection1->Rename("New/collection"));
        CHECK(previousName == collection1->GetName());
        CHECK(db.SelectCollectionByNameAG("New/collection") == nullptr);
    }
}

TEST_CASE("Collection delete works", "[collection][action]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    auto uncategorized = db.SelectCollectionByIDAG(DATABASE_UNCATEGORIZED_COLLECTION_ID);
    REQUIRE(uncategorized);
    CHECK(uncategorized->GetImages().empty());

    const auto name = "Collection 1";
    const auto name2 = "Another collection";

    auto collection1 = db.InsertCollectionAG(name, false);
    REQUIRE(collection1);

    auto collection2 = db.InsertCollectionAG(name2, false);
    REQUIRE(collection2);

    auto image1 = db.InsertTestImage("image1", "hash1");
    REQUIRE(image1);

    auto image2 = db.InsertTestImage("image2", "hash2");
    REQUIRE(image2);

    auto image3 = db.InsertTestImage("image3", "hash3");
    REQUIRE(image3);

    collection1->AddImage(image1);
    collection1->AddImage(image2);

    collection2->AddImage(image3);

    CHECK(!collection1->IsDeleted());

    auto action = db.DeleteCollection(*collection1);

    REQUIRE(action);
    REQUIRE(collection1->IsDeleted());
    CHECK(!collection2->IsDeleted());

    SECTION("Deleted gallery is not found")
    {
        CHECK(db.SelectCollectionByNameAG(name) == nullptr);
    }

    SECTION("Undo and redo work")
    {
        REQUIRE(action->Undo());

        CHECK(!collection1->IsDeleted());
        CHECK(db.SelectCollectionByNameAG(name) == collection1);

        REQUIRE(action->Redo());
        CHECK(collection1->IsDeleted());
        CHECK(db.SelectCollectionByNameAG(name) == nullptr);
    }

    SECTION("Purged deleted images are in uncategorized")
    {
        CHECK(!action->IsDeleted());
        db.PurgeOldActionsUntilSpecificCountAG(0);
        CHECK(action->IsDeleted());

        CHECK(uncategorized->GetImages() == std::vector<std::shared_ptr<Image>>{image1, image2});
    }

    SECTION("Another collection is left untouched")
    {
        CHECK(!collection2->IsDeleted());
        CHECK(db.SelectCollectionByNameAG(name2) == collection2);
        CHECK(collection2->GetImages() == std::vector<std::shared_ptr<Image>>{image3});
    }
}

TEST_CASE("Can't delete uncategorized", "[collection]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    auto uncategorized = db.SelectCollectionByIDAG(DATABASE_UNCATEGORIZED_COLLECTION_ID);
    REQUIRE(uncategorized);

    CHECK_THROWS(db.DeleteCollection(*uncategorized));

    CHECK(!uncategorized->IsDeleted());
}

TEST_CASE("Collection delete purge doesn't add extra stuff to uncategorized", "[collection][action]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    auto uncategorized = db.SelectCollectionByIDAG(DATABASE_UNCATEGORIZED_COLLECTION_ID);
    REQUIRE(uncategorized);

    auto collection1 = db.InsertCollectionAG("Collection 1", false);
    REQUIRE(collection1);

    auto collection2 = db.InsertCollectionAG("Another collection", false);
    REQUIRE(collection2);

    auto image1 = db.InsertTestImage("image1", "hash1");
    REQUIRE(image1);

    auto image2 = db.InsertTestImage("image2", "hash2");
    REQUIRE(image2);

    auto image3 = db.InsertTestImage("image3", "hash3");
    REQUIRE(image3);

    auto image4 = db.InsertTestImage("image4", "hash4");
    REQUIRE(image4);

    collection1->AddImage(image1);
    collection1->AddImage(image2);
    collection1->AddImage(image3);

    collection2->AddImage(image3);

    // A bit not conforming to the model to have image1 in uncategorized already
    uncategorized->AddImage(image1);
    uncategorized->AddImage(image4);

    db.DeleteCollection(*collection1);
    REQUIRE(collection1->IsDeleted());

    db.PurgeOldActionsUntilSpecificCountAG(0);

    CHECK(uncategorized->GetImages() == std::vector<std::shared_ptr<Image>>{image1, image4, image2});
}

TEST_CASE("Undone collection delete doesn't move images", "[collection][action]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    auto uncategorized = db.SelectCollectionByIDAG(DATABASE_UNCATEGORIZED_COLLECTION_ID);
    REQUIRE(uncategorized);

    auto collection1 = db.InsertCollectionAG("Collection 1", false);
    REQUIRE(collection1);

    auto image1 = db.InsertTestImage("image1", "hash1");
    REQUIRE(image1);

    collection1->AddImage(image1);

    auto action = db.DeleteCollection(*collection1);
    REQUIRE(action);
    REQUIRE(collection1->IsDeleted());

    CHECK(action->Undo());
    CHECK(!action->IsPerformed());

    db.PurgeOldActionsUntilSpecificCountAG(0);

    CHECK(!action->IsPerformed());
    CHECK(!collection1->IsDeleted());

    CHECK(uncategorized->GetImages().empty());
}

TEST_CASE("Collection delete with collection no longer in memory works", "[collection][action]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    const auto name = "Collection to delete";

    std::shared_ptr<DatabaseAction> action;
    DBID collectionId;

    {
        auto collection = db.InsertCollectionAG(name, false);
        REQUIRE(collection);
        collectionId = collection->GetID();
        CHECK(!collection->IsDeleted());

        action = db.DeleteCollection(*collection);
        REQUIRE(action);
        REQUIRE(collection->IsDeleted());
    }

    SECTION("Loaded collection preserves deleted flag")
    {
        auto collection = db.SelectCollectionByIDAG(collectionId);
        REQUIRE(collection);
        CHECK(collection->IsDeleted());
    }

    SECTION("Collection is purged correctly")
    {
        CHECK(!action->IsDeleted());
        db.PurgeOldActionsUntilSpecificCountAG(0);
        CHECK(action->IsDeleted());

        auto collection = db.SelectCollectionByIDAG(collectionId);
        CHECK(!collection);
    }
}
