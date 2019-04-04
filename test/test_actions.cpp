#include "catch.hpp"


#include "TestDualView.h"

#include "TestDatabase.h"
#include "resources/Collection.h"
#include "resources/DatabaseAction.h"
#include "resources/Image.h"

using namespace DV;

TEST_CASE("Image delete undo and redo works", "[db][action]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    // Insert test data
    auto image1 = db.InsertTestImage("image1", "hash1");
    REQUIRE(image1);

    auto image2 = db.InsertTestImage("image2", "hash2");
    REQUIRE(image2);

    auto collection = db.InsertCollectionAG("test collection", false);
    REQUIRE(collection);

    collection->AddImage(image1);
    collection->AddImage(image2);

    auto undo = db.DeleteImage(*image1);

    REQUIRE(undo);

    CHECK(collection->GetImages() == std::vector<std::shared_ptr<Image>>{image2});

    CHECK(image1->IsDeleted());
    CHECK(!image2->IsDeleted());

    SECTION("immediate undo")
    {
        CHECK(undo->Undo() == true);

        CHECK(!image1->IsDeleted());

        CHECK(collection->GetImages() == std::vector<std::shared_ptr<Image>>{image1, image2});
    }

    SECTION("action can be loaded from the database and undone")
    {
        const auto id = undo->GetID();
        CHECK(id >= 0);

        const auto oldPtr = undo.get();
        undo.reset();

        undo = db.SelectDatabaseActionByIDAG(id);
        REQUIRE(undo);

        CHECK(undo.get() != oldPtr);
        CHECK(undo->GetID() == id);

        CHECK(undo->Undo() == true);

        CHECK(!image1->IsDeleted());

        CHECK(collection->GetImages() == std::vector<std::shared_ptr<Image>>{image1, image2});
    }
}

TEST_CASE("Purging image delete action permanently deletes Images", "[db][action]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    // Insert test data
    auto image1 = db.InsertTestImage("image1", "hash1");
    REQUIRE(image1);

    const auto id = image1->GetID();

    auto image2 = db.InsertTestImage("image2", "hash2");
    REQUIRE(image2);

    auto collection = db.InsertCollectionAG("test collection", false);
    REQUIRE(collection);

    collection->AddImage(image1);
    collection->AddImage(image2);

    auto undo = db.DeleteImage(*image1);

    REQUIRE(undo);

    db.DeleteDatabaseAction(*undo);
    CHECK(undo->IsDeleted());
    CHECK(image1->IsDeleted());
    CHECK(!image2->IsDeleted());

    CHECK(id == image1->GetID());
    CHECK(!db.SelectImageByIDAG(id));
    CHECK(collection->GetImages() == std::vector<std::shared_ptr<Image>>{image2});
}


TEST_CASE("Deleting undone action doesn't delete Images", "[db][action]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    // Insert test data
    auto image1 = db.InsertTestImage("image1", "hash1");
    REQUIRE(image1);

    const auto id = image1->GetID();

    auto image2 = db.InsertTestImage("image2", "hash2");
    REQUIRE(image2);

    auto collection = db.InsertCollectionAG("test collection", false);
    REQUIRE(collection);

    collection->AddImage(image1);
    collection->AddImage(image2);

    auto undo = db.DeleteImage(*image1);

    REQUIRE(undo);

    CHECK(undo->Undo());

    db.DeleteDatabaseAction(*undo);
    CHECK(undo->IsDeleted());
    CHECK(!image1->IsDeleted());
    CHECK(!image2->IsDeleted());

    CHECK(id == image1->GetID());
    CHECK(db.SelectImageByIDAG(id));

    CHECK(collection->GetImages() == std::vector<std::shared_ptr<Image>>{image1, image2});
}

TEST_CASE("Image merge works correctly", "[db][action]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    // Insert the images for testing
    auto image1 = db.InsertTestImage("image1", "hash1");
    REQUIRE(image1);

    auto image2 = db.InsertTestImage("image2", "hash2");
    REQUIRE(image2);

    const auto id2 = image2->GetID();

    auto image3 = db.InsertTestImage("image3", "hash3");
    REQUIRE(image3);

    SECTION("Basic merge without collections or tags")
    {
        auto undo = db.MergeImages(*image1, {image2});
        REQUIRE(undo);

        CHECK(!image1->IsDeleted());
        CHECK(image2->IsDeleted());

        SECTION("Undo")
        {
            CHECK(undo->Undo());

            CHECK(!image1->IsDeleted());
            CHECK(!image2->IsDeleted());
        }
    }
}
