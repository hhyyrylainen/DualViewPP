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
    std::unique_ptr<Database> dbptr(new TestDatabase());
    DummyDualView dv(std::move(dbptr));
    auto& db = static_cast<TestDatabase&>(dv.GetDatabase());

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

        SECTION("Undo works after loaded from DB")
        {
            const auto id = undo->GetID();
            const auto oldPtr = undo.get();
            undo.reset();

            undo = db.SelectDatabaseActionByIDAG(id);
            REQUIRE(undo);

            CHECK(undo.get() != oldPtr);

            CHECK(undo->Undo());

            CHECK(!image1->IsDeleted());
            CHECK(!image2->IsDeleted());
        }
    }

    SECTION("Tags are merged")
    {
        const auto hair = dv.ParseTagFromString("hair");
        const auto uniform = dv.ParseTagFromString("uniform");
        REQUIRE(hair);
        REQUIRE(uniform);

        auto img1Tags = image1->GetTags();
        REQUIRE(img1Tags);
        CHECK(img1Tags->Add(hair));
        REQUIRE(image2->GetTags());
        CHECK(image2->GetTags()->Add(uniform));
        // Duplicate to see if that causes issues
        CHECK(image2->GetTags()->Add(hair));

        auto undo = db.MergeImages(*image1, {image2});
        REQUIRE(undo);

        CHECK(img1Tags->HasTag(*hair));
        CHECK(img1Tags->HasTag(*uniform));

        SECTION("Undo")
        {
            CHECK(undo->Undo());

            CHECK(img1Tags->HasTag(*hair));
            CHECK(!img1Tags->HasTag(*uniform));
        }

        SECTION("Undo works after loaded from DB")
        {
            const auto oldTags =
                dynamic_cast<ImageMergeAction*>(undo.get())->GetAddTagsToTarget();

            const auto id = undo->GetID();
            const auto oldPtr = undo.get();
            undo.reset();

            undo = db.SelectDatabaseActionByIDAG(id);
            REQUIRE(undo);

            CHECK(undo.get() != oldPtr);

            const auto newTags =
                dynamic_cast<ImageMergeAction*>(undo.get())->GetAddTagsToTarget();

            CHECK(oldTags == newTags);

            CHECK(undo->Undo());

            CHECK(img1Tags->HasTag(*hair));
            CHECK(!img1Tags->HasTag(*uniform));
        }
    }

    SECTION("Contained in collections are merged")
    {
        auto collection1 = db.InsertCollectionAG("test collection", false);
        REQUIRE(collection1);

        auto collection2 = db.InsertCollectionAG("test2", false);
        REQUIRE(collection2);

        collection1->AddImage(image1);
        collection1->AddImage(image2);

        collection2->AddImage(image2);
        collection2->AddImage(image3);

        auto undo = db.MergeImages(*image1, {image2});
        REQUIRE(undo);

        CHECK(collection1->GetImages() == std::vector<std::shared_ptr<Image>>{image1});
        CHECK(collection2->GetImages() == std::vector<std::shared_ptr<Image>>{image1, image3});

        SECTION("Undo")
        {
            CHECK(undo->Undo());

            CHECK(collection1->GetImages() ==
                  std::vector<std::shared_ptr<Image>>{image1, image2});
            CHECK(collection2->GetImages() ==
                  std::vector<std::shared_ptr<Image>>{image2, image3});
        }

        SECTION("Undo works after loaded from DB")
        {
            const auto oldCollections =
                dynamic_cast<ImageMergeAction*>(undo.get())->GetAddTargetToCollections();

            const auto id = undo->GetID();
            const auto oldPtr = undo.get();
            undo.reset();

            undo = db.SelectDatabaseActionByIDAG(id);
            REQUIRE(undo);

            CHECK(undo.get() != oldPtr);

            const auto newCollections =
                dynamic_cast<ImageMergeAction*>(undo.get())->GetAddTargetToCollections();

            CHECK(oldCollections == newCollections);

            CHECK(undo->Undo());

            CHECK(collection1->GetImages() ==
                  std::vector<std::shared_ptr<Image>>{image1, image2});
            CHECK(collection2->GetImages() ==
                  std::vector<std::shared_ptr<Image>>{image2, image3});
        }
    }
}

TEST_CASE("Searching action by description works", "[db][action]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    auto image1 = db.InsertTestImage("image1", "hash1");
    REQUIRE(image1);

    auto undo = db.DeleteImage(*image1);

    const auto found = db.SelectLatestDatabaseActions("deleted");
    REQUIRE(!found.empty());
    CHECK(undo == found[0]);
}

TEST_CASE("Image removal from a collection can be undone", "[db][action]")
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

    auto undo = db.DeleteImagesFromCollection(*collection, {image1});

    REQUIRE(undo);

    CHECK(collection->GetImages() == std::vector<std::shared_ptr<Image>>{image2});

    // Image must always be in some collection
    CHECK(db.SelectIsImageInAnyCollectionAG(*image1));
    CHECK(!image1->IsDeleted());

    SECTION("works normally")
    {
        CHECK(undo->Undo());

        CHECK(collection->GetImages() == std::vector<std::shared_ptr<Image>>{image1, image2});
    }

    SECTION("works after loading from DB")
    {
        const auto id = undo->GetID();
        const auto oldPtr = undo.get();
        undo.reset();

        undo = db.SelectDatabaseActionByIDAG(id);
        REQUIRE(undo);

        CHECK(undo.get() != oldPtr);

        CHECK(undo->Undo());

        CHECK(collection->GetImages() == std::vector<std::shared_ptr<Image>>{image1, image2});

        SECTION("It gets removed from uncategorized")
        {
            auto uncategorized = db.SelectCollectionByID(DATABASE_UNCATEGORIZED_COLLECTION_ID);

            CHECK(uncategorized->GetImages() == std::vector<std::shared_ptr<Image>>{});
        }
    }
}

TEST_CASE("Collection reorder can be undone", "[db][action]")
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

    auto undo = db.UpdateCollectionImagesOrder(*collection, {image2, image1});

    REQUIRE(undo);

    CHECK(collection->GetImages() == std::vector<std::shared_ptr<Image>>{image2, image1});

    SECTION("works normally")
    {
        CHECK(undo->Undo());

        CHECK(collection->GetImages() == std::vector<std::shared_ptr<Image>>{image1, image2});
    }

    SECTION("works after loading from DB")
    {
        const auto id = undo->GetID();
        const auto oldPtr = undo.get();
        undo.reset();

        undo = db.SelectDatabaseActionByIDAG(id);
        REQUIRE(undo);

        CHECK(undo.get() != oldPtr);

        CHECK(undo->Undo());

        CHECK(collection->GetImages() == std::vector<std::shared_ptr<Image>>{image1, image2});
    }
}
