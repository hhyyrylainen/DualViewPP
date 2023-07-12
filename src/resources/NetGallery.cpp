// ------------------------------------ //
#include "NetGallery.h"

#include "resources/InternetImage.h"
#include "resources/Tags.h"

#include "Database.h"
#include "Exceptions.h"

using namespace DV;

// ------------------------------------ //
NetGallery::NetGallery(const std::string& url, const std::string& targetgallery) :
    DatabaseResource(true), GalleryURL(url), TargetGalleryName(targetgallery)
{
}

NetGallery::NetGallery(Database& db, DatabaseLockT& dblock, PreparedStatement& statement, int64_t id) :
    DatabaseResource(id, db)
{
    // Load properties //
    CheckRowID(statement, 1, "gallery_url");
    CheckRowID(statement, 2, "target_path");
    CheckRowID(statement, 3, "gallery_name");
    CheckRowID(statement, 4, "currently_scanned");
    CheckRowID(statement, 5, "is_downloaded");
    CheckRowID(statement, 6, "tags_string");
    CheckRowID(statement, 7, "deleted");

    GalleryURL = statement.GetColumnAsString(1);

    TargetPath = statement.GetColumnAsString(2);

    TargetGalleryName = statement.GetColumnAsString(3);

    CurrentlyScanned = statement.GetColumnAsString(4);

    IsDownloaded = statement.GetColumnAsBool(5);

    TagsString = statement.GetColumnAsString(6);

    Deleted = statement.GetColumnAsOptionalBool(7);
}

NetGallery::~NetGallery()
{
    DBResourceDestruct();
}

// ------------------------------------ //
void NetGallery::AddFilesToDownload(
    const std::vector<std::shared_ptr<InternetImage>>& images, DatabaseLockT& databaselock)
{
    if (!IsInDatabase())
        throw Leviathan::InvalidState("NetGallery not in database");

    for (const auto& image : images)
    {
        std::string tags;

        auto tagsObj = image->GetTags();

        // Make sure it is loaded so that the HasTags call doesn't try to relock the database
        tagsObj->CheckIsLoaded(databaselock);

        if (tagsObj->HasTags())
        {
            tags = tagsObj->TagsAsString(";");
        }

        // TODO: support for cookie storing (cookies should be deleted on download success)
        NetFile file(image->GetURL().GetURL(), image->GetURL().GetReferrer(), image->GetName(), tags);
        InDatabase->InsertNetFile(databaselock, file, *this);
    }
}

void NetGallery::ReplaceItemsWith(
    const std::vector<std::shared_ptr<InternetImage>>& images, DatabaseLockT& databaselock)
{
    if (!IsInDatabase())
        throw Leviathan::InvalidState("NetGallery not in database");

    DoDBSavePoint transaction(*InDatabase, databaselock, "netgallery_replace_items");
    transaction.AllowCommit(false);

    auto existing = InDatabase->SelectNetFilesFromGallery(*this);

    // TODO: this can't be reversed
    for (const auto& item : existing)
    {
        // NetFile cannot be on its own so it must be deleted here
        InDatabase->DeleteNetFile(*item);
    }

    AddFilesToDownload(images, databaselock);
    transaction.AllowCommit(true);
}

// ------------------------------------ //
void NetGallery::_DoSave(Database& db)
{
    db.UpdateNetGallery(*this);
}

// ------------------------------------ //
// NetFile
NetFile::NetFile(
    const std::string& url, const std::string& referrer, const std::string& name, const std::string& tagstr /*= ""*/) :
    DatabaseResource(true),
    FileURL(url), PageReferrer(referrer), PreferredName(name), TagsString(tagstr)
{
}

NetFile::NetFile(Database& db, DatabaseLockT& dblock, PreparedStatement& statement, int64_t id) :
    DatabaseResource(id, db)
{
    // Load properties //
    CheckRowID(statement, 1, "file_url");
    CheckRowID(statement, 2, "page_referrer");
    CheckRowID(statement, 3, "preferred_name");
    CheckRowID(statement, 4, "tags_string");

    FileURL = statement.GetColumnAsString(1);
    PageReferrer = statement.GetColumnAsString(2);
    PreferredName = statement.GetColumnAsString(3);
    TagsString = statement.GetColumnAsString(4);
}

NetFile::~NetFile()
{
    DBResourceDestruct();
}

// ------------------------------------ //
void NetFile::_DoSave(Database& db)
{
    db.UpdateNetFile(*this);
}
