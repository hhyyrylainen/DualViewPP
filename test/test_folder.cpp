#include "catch.hpp"

#include "TestDatabase.h"
#include "TestDualView.h"

#include "resources/DatabaseAction.h"
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

    const auto initialRootContents = db.SelectCollectionsInFolderAG(*root);

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

    SECTION("Basic add")
    {
        CHECK(db.SelectFolderByNameAndParentAG("Another", *folder1) == nullptr);
        CHECK(folder1->AddFolder(folder2));
        CHECK(db.SelectFolderByNameAndParentAG("Another", *folder1) == folder2);
    }

    SECTION("Can't add conflicting name")
    {
        CHECK(db.SelectFolderByNameAndParentAG("The new thing", *folder2) == folder3);
        CHECK(!folder2->AddFolder(folder4));
        CHECK(db.SelectFolderByNameAndParentAG("The new thing", *folder2) == folder3);
    }
}

TEST_CASE("Remove folder from folder works", "[folder]")
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

    CHECK(folder1->AddFolder(folder2));

    SECTION("Basic remove")
    {
        CHECK(db.SelectFolderByNameAndParentAG("Another", *folder1) == folder2);
        CHECK(folder1->RemoveFolder(folder2));
        CHECK(db.SelectFolderByNameAndParentAG("Another", *folder1) == nullptr);
    }

    SECTION("Folder that is nowhere is added to root")
    {
        CHECK(root->RemoveFolder(folder2));
        CHECK(db.SelectFolderByNameAndParentAG("Another", *root) == nullptr);
        CHECK(folder1->RemoveFolder(folder2));
        CHECK(db.SelectFolderByNameAndParentAG("Another", *root) == folder2);
    }
}

TEST_CASE("Delete folder works", "[folder][action]")
{
    DummyDualView dv(std::make_unique<TestDatabase>());
    Database& db = dv.GetDatabase();

    REQUIRE_NOTHROW(db.Init());

    auto root = db.SelectFolderByIDAG(DATABASE_ROOT_FOLDER_ID);
    REQUIRE(root);

    auto folder1 = db.InsertFolder("Folder 1", false, *root);
    REQUIRE(folder1);
    auto folder2 = db.InsertFolder("Another", false, *root);
    auto folder3 = db.InsertFolder("Subfolder", false, *folder1);

    auto collection1 = db.InsertCollectionAG("Collection 1", false);
    auto collection2 = db.InsertCollectionAG("Collection 2", false);
    auto collection3 = db.InsertCollectionAG("in Root collection", false);

    dv.AddCollectionToFolder(folder1, collection1);
    dv.AddCollectionToFolder(folder2, collection2);

    const auto initialRootContents = db.SelectCollectionsInFolderAG(*root);

    CHECK(std::find(initialRootContents.begin(), initialRootContents.end(), collection3) !=
          initialRootContents.end());
    CHECK(std::find(initialRootContents.begin(), initialRootContents.end(), collection1) ==
          initialRootContents.end());

    // Hopefully this order stays consistent in sqlite...
    const auto initialRootContainedFolders =
        std::vector<std::shared_ptr<Folder>>{folder2, folder1};

    CHECK(db.SelectFoldersInFolderAG(*root) == initialRootContainedFolders);

    auto action = db.DeleteFolder(*folder1);
    REQUIRE(action);
    CHECK(action->IsPerformed());
    CHECK(folder1->IsDeleted());

    CHECK(db.SelectFoldersInFolderAG(*root) ==
          std::vector<std::shared_ptr<Folder>>{folder2, folder3});

    SECTION("Orphaned collections are moved to root")
    {
        const auto newRootContents = db.SelectCollectionsInFolderAG(*root);
        CHECK(std::find(newRootContents.begin(), newRootContents.end(), collection1) !=
              newRootContents.end());
        CHECK(std::find(newRootContents.begin(), newRootContents.end(), collection2) ==
              newRootContents.end());
    }

    SECTION("Undo works")
    {
        CHECK(action->Undo());
        CHECK(!folder1->IsDeleted());

        const auto newRootContents = db.SelectCollectionsInFolderAG(*root);
        CHECK(std::find(newRootContents.begin(), newRootContents.end(), collection1) ==
              newRootContents.end());
        CHECK(std::find(newRootContents.begin(), newRootContents.end(), collection2) ==
              newRootContents.end());

        CHECK(db.SelectFoldersInFolderAG(*root) == initialRootContainedFolders);
    }
}

TEST_CASE("Undoing folder delete doesn't cause name conflict", "[folder][action]")
{
    DummyDualView dv(std::make_unique<TestDatabase>());
    Database& db = dv.GetDatabase();

    REQUIRE_NOTHROW(db.Init());

    auto root = db.SelectFolderByIDAG(DATABASE_ROOT_FOLDER_ID);
    REQUIRE(root);

    auto folder1 = db.InsertFolder("Folder 1", false, *root);
    REQUIRE(folder1);

    CHECK(db.SelectFoldersInFolderAG(*root) == std::vector<std::shared_ptr<Folder>>{folder1});

    auto action = db.DeleteFolder(*folder1);
    REQUIRE(action);
    CHECK(action->IsPerformed());

    CHECK(db.SelectFoldersInFolderAG(*root) == std::vector<std::shared_ptr<Folder>>{});

    auto folder2 = db.InsertFolder("Folder 1", false, *root);
    REQUIRE(folder2);

    CHECK(folder1 != folder2);

    CHECK(db.SelectFoldersInFolderAG(*root) == std::vector<std::shared_ptr<Folder>>{folder2});

    CHECK_THROWS(action->Undo());

    CHECK(db.SelectFoldersInFolderAG(*root) == std::vector<std::shared_ptr<Folder>>{folder2});
}

TEST_CASE("Can't delete a folder that would cause name conflict in root", "[folder][action]")
{
    DummyDualView dv(std::make_unique<TestDatabase>());
    Database& db = dv.GetDatabase();

    REQUIRE_NOTHROW(db.Init());

    auto root = db.SelectFolderByIDAG(DATABASE_ROOT_FOLDER_ID);
    REQUIRE(root);

    auto folder1 = db.InsertFolder("Folder", false, *root);
    auto folder2 = db.InsertFolder("Folder 2", false, *root);
    auto folder3 = db.InsertFolder("Folder", false, *folder2);

    // Collections don't need to be tested as their names are globally unique always

    {
        const auto rootContents = db.SelectFoldersInFolderAG(*root);
        CHECK(std::find(rootContents.begin(), rootContents.end(), folder1) !=
              rootContents.end());
        CHECK(std::find(rootContents.begin(), rootContents.end(), folder2) !=
              rootContents.end());
        CHECK(std::find(rootContents.begin(), rootContents.end(), folder3) ==
              rootContents.end());
    }

    std::shared_ptr<DatabaseAction> action;
    CHECK_THROWS(action = db.DeleteFolder(*folder2));
    CHECK(!action);
    CHECK(!folder2->IsDeleted());

    {
        const auto rootContents = db.SelectFoldersInFolderAG(*root);
        CHECK(std::find(rootContents.begin(), rootContents.end(), folder1) !=
              rootContents.end());
        CHECK(std::find(rootContents.begin(), rootContents.end(), folder2) !=
              rootContents.end());
        CHECK(std::find(rootContents.begin(), rootContents.end(), folder3) ==
              rootContents.end());
    }
}
