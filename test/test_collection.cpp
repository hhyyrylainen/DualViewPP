#include "catch.hpp"

#include "TestDualView.h"

#include "DummyLog.h"
#include "TestDatabase.h"
#include "resources/Collection.h"

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

    SECTION("Conflict disallows rename") {

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
