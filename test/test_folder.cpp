#include "catch.hpp"

#include "TestDatabase.h"
#include "TestDualView.h"

#include "resources/Folder.h"

using namespace DV;

TEST_CASE("Folder rename works", "[folder]")
{
    DummyDualView dv;
    TestDatabase db;

    REQUIRE_NOTHROW(db.Init());

    auto root = db.SelectFolderByIDAG(DATABASE_ROOT_FOLDER_ID);
    REQUIRE(root);

    auto folder1 = db.InsertFolder("Folder 1", false, *root);
    auto folder2 = db.InsertFolder("Another", false, *root);

    CHECK(db.SelectFolderByNameAndParentAG("Folder 1", *root) == folder1);

    SECTION("Simple rename")
    {
        REQUIRE(folder1->Rename("New things"));
        CHECK(folder1->GetName() == "New things");
        CHECK(db.SelectFolderByNameAndParentAG("New things", *root) == folder1);
    }

    SECTION("Can't rename to existing name")
    {
        CHECK(!folder1->Rename("Another"));
        CHECK(folder1->GetName() == "Folder 1");
        CHECK(db.SelectFolderByNameAndParentAG("Folder 1", *root) == folder1);
    }

    SECTION("Can't rename add '/'")
    {
        CHECK(!folder1->Rename("New/name"));
        CHECK("Folder 1" == folder1->GetName());
        CHECK(db.SelectFolderByNameAndParentAG("New/name", *root) == nullptr);
    }
}

TEST_CASE("Folder creation and adding collections works as expected", "[folder]")
{
    DummyDualView dv(std::make_unique<TestDatabase>());
    Database& db = dv.GetDatabase();

    REQUIRE_NOTHROW(db.Init());

    auto root = db.SelectFolderByIDAG(DATABASE_ROOT_FOLDER_ID);
    REQUIRE(root);

    auto folder1 = db.InsertFolder("Folder 1", false, *root);
    auto folder2 = db.InsertFolder("Another", false, *root);

    auto collection1 = db.InsertCollectionAG("Collection 1", false);
    auto collection2 = db.InsertCollectionAG("Collection 2", false);
    auto collection3 = db.InsertCollectionAG("in Root collection", false);

    dv.AddCollectionToFolder(folder1, collection1);
    dv.AddCollectionToFolder(folder2, collection2);

    const auto initialRootContents = db.SelectCollectionsInFolder(*root);

    CHECK(std::find(initialRootContents.begin(), initialRootContents.end(), collection1) ==
          initialRootContents.end());
    CHECK(std::find(initialRootContents.begin(), initialRootContents.end(), collection2) ==
          initialRootContents.end());
    CHECK(std::find(initialRootContents.begin(), initialRootContents.end(), collection3) !=
          initialRootContents.end());
}

TEST_CASE("Add folder to folder works", "[folder]")
{
    DummyDualView dv(std::make_unique<TestDatabase>());
    Database& db = dv.GetDatabase();

    REQUIRE_NOTHROW(db.Init());

    auto root = db.SelectFolderByIDAG(DATABASE_ROOT_FOLDER_ID);
    REQUIRE(root);

    auto folder1 = db.InsertFolder("Folder 1", false, *root);
    auto folder2 = db.InsertFolder("Another", false, *root);
    auto folder3 = db.InsertFolder("The new thing", false, *folder2);
    auto folder4 = db.InsertFolder("The new thing", false, *root);

    CHECK("Basic add") {


    }

    CHECK("Can't add conflicting name") {}
}
