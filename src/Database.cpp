// ------------------------------------ //
#include "Database.h"
#include "Common.h"
#include "Exceptions.h"

#include "PreparedStatement.h"

#include "resources/Collection.h"
#include "resources/Folder.h"
#include "resources/Image.h"
#include "resources/NetGallery.h"
#include "resources/Tags.h"

#include "CacheManager.h"
#include "TimeHelpers.h"
#include "UtilityHelpers.h"

#include "ChangeEvents.h"
#include "DualView.h"

#include "CurlWrapper.h"

#include "Common/StringOperations.h"

#include <sqlite3.h>

#include <boost/filesystem.hpp>

#include <thread>

using namespace DV;
// ------------------------------------ //
Database::Database(std::string dbfile) : DatabaseFile(dbfile)
{
    if(dbfile.empty())
        throw Leviathan::InvalidArgument("dbfile is empty");

#ifdef _WIN32
    // This needs to be set to work properly on windows
    sqlite3_temp_directory();

#endif // _WIN32

    CurlWrapper urlencoder;
    char* curlEncoded = curl_easy_escape(urlencoder.Get(), dbfile.c_str(), dbfile.size());

    dbfile = std::string(curlEncoded);

    curl_free(curlEncoded);

    // If begins with ':' add a ./ to the beginning
    // as recommended by sqlite documentation
    if(dbfile[0] == ':')
        dbfile = "./" + dbfile;

    // Add the file uri specifier
    dbfile = "file:" + dbfile;

    // Open with SQLITE_OPEN_NOMUTEX because we already use explicit mutex locks
    const auto result = sqlite3_open_v2(dbfile.c_str(), &SQLiteDb,
        SQLITE_OPEN_URI | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX,
        nullptr);

    if(!SQLiteDb) {

        throw Leviathan::InvalidState("failed to allocate memory for sqlite database");
    }

    if(result != SQLITE_OK) {

        const std::string errormessage(sqlite3_errmsg(SQLiteDb));

        sqlite3_close(SQLiteDb);
        SQLiteDb = nullptr;
        LOG_ERROR("Sqlite failed to open database '" + dbfile +
                  "' errorcode: " + Convert::ToString(result) + " message: " + errormessage);
        throw Leviathan::InvalidState("failed to open sqlite database");
    }
}

Database::Database(bool tests)
{

    LEVIATHAN_ASSERT(tests, "Database test version not constructed with true");

    // const auto result = sqlite3_open(":memory:", &SQLiteDb);
    const auto result = sqlite3_open_v2(":memory:", &SQLiteDb,
        SQLITE_OPEN_URI | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX,
        nullptr);

    if(result != SQLITE_OK || SQLiteDb == nullptr) {

        sqlite3_close(SQLiteDb);
        SQLiteDb = nullptr;
        throw Leviathan::InvalidState("failed to open memory sqlite database");
    }
}

Database::~Database()
{

    GUARD_LOCK();
    // No operations can be in progress, as we are locked
    // But if there were that would be an error in DualView not properly
    // shutting everything down


    // Stop all active operations //


    // Release all prepared objects //


    if(SQLiteDb) {

        while(sqlite3_close(SQLiteDb) != SQLITE_OK) {

            LOG_WARNING("Database waiting for sqlite3 resources to be released, "
                        "database cannot be closed yet");
        }

        SQLiteDb = nullptr;
    }
}
// ------------------------------------ //
void Database::Init()
{

    GUARD_LOCK();

    {
        const char str[] =
            "PRAGMA foreign_keys = ON; PRAGMA recursive_triggers = ON; "
            // Note if this is changed also places where journal_mode is restored
            //! need to be updated
            "PRAGMA journal_mode = DELETE;";

        PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

        auto statementinuse = statementobj.Setup();

        statementobj.StepAll(statementinuse);
    }

    // Verify foreign keys are on //
    {
        const char str[] = "PRAGMA foreign_keys; PRAGMA recursive_triggers;";

        PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

        auto statementinuse = statementobj.Setup();

        while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

            if(statementobj.GetColumnAsInt(0) != 1) {

                throw Leviathan::InvalidState("Foreign keys didn't get enabled");
            }
        }
    }

    // Verify database version and setup tables if they don't exist //
    int fileVersion = -1;

    if(!SelectDatabaseVersion(guard, fileVersion)) {

        // Database is newly created //
        _CreateTableStructure(guard);

    } else {

        // Check that the version is compatible, upgrade if needed //
        if(!_VerifyLoadedVersion(guard, fileVersion)) {

            throw Leviathan::InvalidState("Database file is unsupported version");
        }
    }

    // Setup statements //
}

void Database::PurgeInactiveCache()
{

    GUARD_LOCK();

    LoadedCollections.Purge();
    LoadedImages.Purge();
    LoadedFolders.Purge();
    LoadedTags.Purge();
    LoadedNetGalleries.Purge();
}
// ------------------------------------ //
bool Database::SelectDatabaseVersion(Lock& guard, int& result)
{

    const char str[] = "SELECT number FROM version;";

    try {

        PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

        auto statementinuse = statementobj.Setup();

        if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

            result = statementobj.GetColumnAsInt(0);
            return true;
        }

    } catch(const InvalidSQL&) {

        // No version info
        result = -1;
        return false;
    }

    // There is no version //
    result = -1;
    return false;
}
// ------------------------------------ //
// Image
void Database::InsertImage(Lock& guard, Image& image)
{

    LEVIATHAN_ASSERT(image.IsReady(), "InsertImage: image not ready");

    const char str[] = "INSERT INTO pictures (relative_path, width, height, name, extension, "
                       "add_date, last_view, is_private, from_file, file_hash) VALUES "
                       "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(
        CacheManager::GetDatabaseImagePath(image.GetResourcePath()), image.GetWidth(),
        image.GetHeight(), image.GetName(), image.GetExtension(), image.GetAddDateStr(),
        image.GetLastViewStr(), image.GetIsPrivate(), image.GetFromFile(), image.GetHash());

    statementobj.StepAll(statementinuse);

    const DBID id = SelectImageIDByHash(guard, image.GetHash());

    image.OnAdopted(id, *this);
}

bool Database::UpdateImage(const Image& image)
{

    GUARD_LOCK();

    // Don't forget to call CacheManager::GetDatabaseImagePath when saving the path
    return false;
}

bool Database::DeleteImage(Image& image)
{

    GUARD_LOCK();
    return false;
}

DBID Database::SelectImageIDByHash(Lock& guard, const std::string& hash)
{

    const char str[] = "SELECT id FROM pictures WHERE file_hash = ?1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(hash);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID id;
        if(statementobj.GetObjectIDFromColumn(id, 0))
            return id;
    }

    return -1;
}

std::shared_ptr<Image> Database::SelectImageByHash(Lock& guard, const std::string& hash)
{

    const char str[] = "SELECT * FROM pictures WHERE file_hash = ?1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(hash);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return _LoadImageFromRow(guard, statementobj);
    }

    return nullptr;
}

std::shared_ptr<Image> Database::SelectImageByID(Lock& guard, DBID id)
{

    const char str[] = "SELECT * FROM pictures WHERE id = ?1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(id);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return _LoadImageFromRow(guard, statementobj);
    }

    return nullptr;
}

std::vector<std::shared_ptr<Image>> Database::SelectImageByTag(Lock& guard, DBID tagid)
{
    std::vector<std::shared_ptr<Image>> result;

    const char str[] = "SELECT * FROM pictures WHERE pictures.id IN "
                       "(SELECT image_tag.image FROM image_tag WHERE image_tag.tag = ?1);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(tagid);

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        result.push_back(_LoadImageFromRow(guard, statementobj));
    }

    return result;
}
// ------------------------------------ //
std::shared_ptr<TagCollection> Database::LoadImageTags(const std::shared_ptr<Image>& image)
{

    if(!image || !image->IsInDatabase())
        return nullptr;

    std::weak_ptr<Image> weak = image;

    auto tags = std::make_shared<DatabaseTagCollection>(
        std::bind(&Database::SelectImageTags, this, std::placeholders::_1, weak,
            std::placeholders::_2),
        std::bind(&Database::InsertImageTag, this, std::placeholders::_1, weak,
            std::placeholders::_2),
        std::bind(&Database::DeleteImageTag, this, std::placeholders::_1, weak,
            std::placeholders::_2),
        *this);

    return tags;
}

void Database::SelectImageTags(
    Lock& guard, std::weak_ptr<Image> image, std::vector<std::shared_ptr<AppliedTag>>& tags)
{
    auto imageLock = image.lock();

    if(!imageLock)
        return;

    const char str[] = "SELECT tag FROM image_tag WHERE image = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(imageLock->GetID());

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID id;

        if(statementobj.GetObjectIDFromColumn(id, 0)) {

            // Load tag //
            auto tag = SelectAppliedTagByID(guard, id);

            if(!tag) {

                LOG_ERROR("Loading AppliedTag for image, no tag with id exists: " +
                          Convert::ToString(id));
                continue;
            }

            tags.push_back(tag);
        }
    }
}

void Database::InsertImageTag(Lock& guard, std::weak_ptr<Image> image, AppliedTag& tag)
{
    auto imageLock = image.lock();

    if(!imageLock)
        return;

    auto existing = SelectExistingAppliedTag(guard, tag);

    if(existing) {

        InsertTagImage(guard, *imageLock, existing->GetID());
        return;
    }

    // Need to create a new tag //
    if(!InsertAppliedTag(guard, tag)) {

        throw InvalidSQL("Failed to create AppliedTag for adding to resource", 0, "");
    }

    InsertTagImage(guard, *imageLock, tag.GetID());
}

void Database::InsertTagImage(Lock& guard, Image& image, DBID appliedtagid)
{

    const char str[] = "INSERT INTO image_tag (image, tag) VALUES (?, ?);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(image.GetID(), appliedtagid);

    statementobj.StepAll(statementinuse);
}

void Database::DeleteImageTag(Lock& guard, std::weak_ptr<Image> image, AppliedTag& tag)
{
    auto imageLock = image.lock();

    if(!imageLock)
        return;

    const char str[] = "DELETE FROM image_tag WHERE image = ? AND tag = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(imageLock->GetID(), tag.GetID());

    statementobj.StepAll(statementinuse);

    // This calls orphan on the tag object
    DeleteAppliedTagIfNotUsed(guard, tag);
}
// ------------------------------------ //
// Collection
std::shared_ptr<Collection> Database::InsertCollection(
    Lock& guard, const std::string& name, bool isprivate)
{
    const char str[] = "INSERT INTO collections (name, is_private, "
                       "add_date, modify_date, last_view) VALUES (?, ?, ?, ?, ?);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    const auto currentTime = TimeHelpers::FormatCurrentTimeAs8601();
    auto statementinuse =
        statementobj.Setup(name, isprivate, currentTime, currentTime, currentTime);

    try {

        statementobj.StepAll(statementinuse);

    } catch(const InvalidSQL& e) {

        LOG_WARNING("Failed to InsertCollection: ");
        e.PrintToLog();
        return nullptr;
    }

    auto created = SelectCollectionByName(guard, name);

    // Add it to the root folder //
    if(!InsertCollectionToFolder(guard, *SelectRootFolder(guard), *created)) {

        LOG_ERROR("Failed to add a new Collection to the root folder");
    }

    DualView::Get().QueueDBThreadFunction(
        []() { DualView::Get().GetEvents().FireEvent(CHANGED_EVENT::COLLECTION_CREATED); });

    return created;
}

bool Database::UpdateCollection(const Collection& collection)
{

    GUARD_LOCK();
    return false;
}

bool Database::DeleteCollection(Collection& collection)
{

    GUARD_LOCK();
    return false;
}

std::shared_ptr<Collection> Database::SelectCollectionByID(DBID id)
{

    GUARD_LOCK();

    const char str[] = "SELECT * FROM collections WHERE id = ?1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(id);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        // Found a value //
        return _LoadCollectionFromRow(guard, statementobj);
    }

    return nullptr;
}

std::shared_ptr<Collection> Database::SelectCollectionByName(
    Lock& guard, const std::string& name)
{
    const char str[] = "SELECT * FROM collections WHERE name = ?1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(name);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        // Found a value //
        return _LoadCollectionFromRow(guard, statementobj);
    }

    return nullptr;
}

std::vector<std::string> Database::SelectCollectionNamesByWildcard(
    const std::string& pattern, int64_t max /*= 50*/)
{
    GUARD_LOCK();

    std::vector<std::string> result;

    const char str[] = "SELECT name FROM collections WHERE name LIKE ?1 "
                       "ORDER BY NAME LIMIT ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup("%" + pattern + "%", max);

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        // Found a value //
        result.push_back(statementobj.GetColumnAsString(0));
    }

    return result;
}

int64_t Database::SelectCollectionLargestShowOrder(Lock& guard, const Collection& collection)
{

    if(!collection.IsInDatabase())
        return 0;

    const char str[] = "SELECT show_order FROM collection_image WHERE collection = ?1 "
                       "ORDER BY show_order DESC LIMIT 1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID());

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return statementobj.GetColumnAsInt64(0);
    }

    return 0;
}

int64_t Database::SelectCollectionImageCount(Lock& guard, const Collection& collection)
{

    if(!collection.IsInDatabase())
        return 0;

    const char str[] = "SELECT COUNT(*) FROM collection_image WHERE collection = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID());

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return statementobj.GetColumnAsInt64(0);
    }

    return 0;
}
// ------------------------------------ //
std::shared_ptr<TagCollection> Database::LoadCollectionTags(
    const std::shared_ptr<Collection>& collection)
{
    if(!collection || !collection->IsInDatabase())
        return nullptr;

    std::weak_ptr<Collection> weak = collection;

    auto tags = std::make_shared<DatabaseTagCollection>(
        std::bind(&Database::SelectCollectionTags, this, std::placeholders::_1, weak,
            std::placeholders::_2),
        std::bind(&Database::InsertCollectionTag, this, std::placeholders::_1, weak,
            std::placeholders::_2),
        std::bind(&Database::DeleteCollectionTag, this, std::placeholders::_1, weak,
            std::placeholders::_2),
        *this);

    return tags;
}

void Database::SelectCollectionTags(Lock& guard, std::weak_ptr<Collection> collection,
    std::vector<std::shared_ptr<AppliedTag>>& tags)
{
    auto collectionLock = collection.lock();

    if(!collectionLock)
        return;

    const char str[] = "SELECT tag FROM collection_tag WHERE collection = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collectionLock->GetID());

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID id;

        if(statementobj.GetObjectIDFromColumn(id, 0)) {

            // Load tag //
            auto tag = SelectAppliedTagByID(guard, id);

            if(!tag) {

                LOG_ERROR("Loading AppliedTag for collection, no tag with id exists: " +
                          Convert::ToString(id));
                continue;
            }

            tags.push_back(tag);
        }
    }
}

void Database::InsertCollectionTag(
    Lock& guard, std::weak_ptr<Collection> collection, AppliedTag& tag)
{
    auto collectionLock = collection.lock();

    if(!collectionLock)
        return;

    auto existing = SelectExistingAppliedTag(guard, tag);

    if(existing) {

        InsertTagCollection(guard, *collectionLock, existing->GetID());
        return;
    }

    // Need to create a new tag //
    if(!InsertAppliedTag(guard, tag)) {

        throw InvalidSQL("Failed to create AppliedTag for adding to resource", 0, "");
    }

    InsertTagCollection(guard, *collectionLock, tag.GetID());
}

void Database::InsertTagCollection(Lock& guard, Collection& collection, DBID appliedtagid)
{

    const char str[] = "INSERT INTO collection_tag (collection, tag) VALUES (?, ?);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID(), appliedtagid);

    statementobj.StepAll(statementinuse);
}

void Database::DeleteCollectionTag(
    Lock& guard, std::weak_ptr<Collection> collection, AppliedTag& tag)
{
    auto collectionLock = collection.lock();

    if(!collectionLock)
        return;

    const char str[] = "DELETE FROM collection_tag WHERE collection = ? AND tag = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collectionLock->GetID(), tag.GetID());

    statementobj.StepAll(statementinuse);

    // This calls orphan on the tag object
    DeleteAppliedTagIfNotUsed(guard, tag);
}
// ------------------------------------ //
// Collection image
bool Database::InsertImageToCollection(
    Lock& guard, Collection& collection, Image& image, int64_t showorder)
{
    if(!collection.IsInDatabase() || !image.IsInDatabase())
        return false;

    const char str[] = "INSERT INTO collection_image (collection, image, show_order) VALUES "
                       "(?1, ?2, ?3);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID(), image.GetID(), showorder);

    statementobj.StepAll(statementinuse);

    const auto changes = sqlite3_changes(SQLiteDb);

    LEVIATHAN_ASSERT(changes <= 1, "InsertImageToCollection changed more than one row");

    return changes == 1;
}

bool Database::SelectIsImageInAnyCollection(Lock& guard, const Image& image)
{

    const char str[] = "SELECT 1 FROM collection_image WHERE image = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(image.GetID());

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return true;
    }

    return false;
}

bool Database::DeleteImageFromCollection(Lock& guard, Collection& collection, Image& image)
{

    if(!collection.IsInDatabase() || !image.IsInDatabase())
        return false;

    const char str[] = "DELETE FROM collection_image WHERE collection = ?1 AND image = ?2;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID(), image.GetID());

    statementobj.StepAll(statementinuse);

    const auto changes = sqlite3_changes(SQLiteDb);

    LEVIATHAN_ASSERT(changes <= 1, "DeleteImageFromCollection changed more than one row");

    return changes == 1;
}

int64_t Database::SelectImageShowOrderInCollection(
    Lock& guard, const Collection& collection, const Image& image)
{
    if(!collection.IsInDatabase() || !image.IsInDatabase())
        return -1;

    const char str[] = "SELECT show_order FROM collection_image WHERE collection = ? AND "
                       "image = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID(), image.GetID());

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return statementobj.GetColumnAsInt64(0);
    }

    return -1;
}

std::shared_ptr<Image> Database::SelectImageInCollectionByShowOrder(
    Lock& guard, const Collection& collection, int64_t showorder)
{
    if(!collection.IsInDatabase())
        return nullptr;

    const char str[] = "SELECT image FROM collection_image WHERE collection = ? AND "
                       "show_order = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID(), showorder);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID id;
        if(statementobj.GetObjectIDFromColumn(id, 0))
            return SelectImageByID(guard, id);
    }

    return nullptr;
}

std::shared_ptr<Image> Database::SelectCollectionPreviewImage(const Collection& collection)
{

    GUARD_LOCK();

    const char str[] = "SELECT preview_image FROM collections WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID());

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID preview;

        if(statementobj.GetObjectIDFromColumn(preview, 0)) {

            // It was set //
            return SelectImageByID(guard, preview);
        }
    }

    // There wasn't a specifically set preview image
    return SelectFirstImageInCollection(guard, collection);
}

std::shared_ptr<Image> Database::SelectFirstImageInCollection(
    Lock& guard, const Collection& collection)
{
    const char str[] = "SELECT image FROM collection_image WHERE collection = ? "
                       "ORDER BY show_order ASC LIMIT 1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID());

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID id;

        if(statementobj.GetObjectIDFromColumn(id, 0))
            return SelectImageByID(guard, id);
    }

    return nullptr;
}

std::shared_ptr<Image> Database::SelectLastImageInCollection(
    Lock& guard, const Collection& collection)
{
    const char str[] = "SELECT image FROM collection_image WHERE collection = ? "
                       "ORDER BY show_order DESC LIMIT 1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID());

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID id;

        if(statementobj.GetObjectIDFromColumn(id, 0))
            return SelectImageByID(guard, id);
    }

    return nullptr;
}

int64_t Database::SelectImageShowIndexInCollection(
    const Collection& collection, const Image& image)
{
    GUARD_LOCK();

    const char str[] =
        "SELECT COUNT(*) FROM collection_image WHERE collection = ?1 "
        "AND show_order < ( SELECT show_order FROM collection_image WHERE collection = ?1 AND "
        "image = ?2 );";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID(), image.GetID());

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return statementobj.GetColumnAsInt64(0);
    }

    // Image wasn't in collection //
    return -1;
}

std::shared_ptr<Image> Database::SelectImageInCollectionByShowIndex(
    Lock& guard, const Collection& collection, int64_t index)
{
    const char str[] = "SELECT * FROM collection_image WHERE collection = ? ORDER BY "
                       "show_order LIMIT 1 OFFSET ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID(), index);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID id;

        if(statementobj.GetObjectIDFromColumn(id, 0))
            return SelectImageByID(guard, id);
    }

    return nullptr;
}

std::shared_ptr<Image> Database::SelectNextImageInCollectionByShowOrder(
    const Collection& collection, int64_t showorder)
{
    GUARD_LOCK();

    const char str[] = "SELECT image FROM collection_image WHERE collection = ?1 "
                       "AND show_order - ?2 > 0 ORDER BY ABS(show_order - ?2) LIMIT 1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID(), showorder);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID id;

        if(statementobj.GetObjectIDFromColumn(id, 0)) {

            return SelectImageByID(guard, id);
        }
    }

    return nullptr;
}

std::shared_ptr<Image> Database::SelectPreviousImageInCollectionByShowOrder(
    const Collection& collection, int64_t showorder)
{
    GUARD_LOCK();

    const char str[] = "SELECT image FROM collection_image WHERE collection = ?1 "
                       "AND show_order - ?2 < 0 ORDER BY ABS(show_order - ?2) LIMIT 1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID(), showorder);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID id;

        if(statementobj.GetObjectIDFromColumn(id, 0)) {

            return SelectImageByID(guard, id);
        }
    }

    return nullptr;
}

std::vector<std::shared_ptr<Image>> Database::SelectImagesInCollection(
    const Collection& collection)
{
    GUARD_LOCK();

    std::vector<std::shared_ptr<Image>> result;

    const char str[] = "SELECT image FROM collection_image WHERE collection = ? "
                       "ORDER BY show_order ASC;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID());

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID id;

        if(statementobj.GetObjectIDFromColumn(id, 0)) {

            result.push_back(SelectImageByID(guard, id));
        }
    }

    return result;
}

// ------------------------------------ //
size_t Database::CountExistingTags()
{

    GUARD_LOCK();

    const char str[] = "SELECT COUNT(*) FROM tags;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup();

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return statementobj.GetColumnAsInt64(0);
    }

    return 0;
}
// ------------------------------------ //
// Folder
std::shared_ptr<Folder> Database::SelectRootFolder(Lock& guard)
{

    const char str[] = "SELECT * FROM virtual_folders WHERE id = 1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup();

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return _LoadFolderFromRow(guard, statementobj);
    }

    LEVIATHAN_ASSERT(false, "Root folder is missing from the database");
    return nullptr;
}

std::shared_ptr<Folder> Database::SelectFolderByID(Lock& guard, DBID id)
{

    const char str[] = "SELECT * FROM virtual_folders WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(id);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return _LoadFolderFromRow(guard, statementobj);
    }

    return nullptr;
}

std::shared_ptr<Folder> Database::InsertFolder(
    std::string name, bool isprivate, const Folder& parent)
{
    // Sanitize name //
    Leviathan::StringOperations::ReplaceSingleCharacter<std::string>(name, "\\/", ' ');

    if(name.empty()) {

        throw InvalidSQL("InsertFolder name is empty", 1, "");
    }

    GUARD_LOCK();

    // Make sure it isn't there already //
    if(SelectFolderByNameAndParent(guard, name, parent))
        return nullptr;

    const char str[] = "INSERT INTO virtual_folders (name, is_private) VALUES "
                       "(?1, ?2);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(name, isprivate);

    statementobj.StepAll(statementinuse);

    const auto id = sqlite3_last_insert_rowid(SQLiteDb);

    auto created = SelectFolderByID(guard, id);

    LEVIATHAN_ASSERT(created, "InsertFolder failed to retrive folder after inserting");

    InsertFolderToFolder(guard, *created, parent);
    return created;
}

bool Database::UpdateFolder(Folder& folder)
{

    GUARD_LOCK();
    return false;
}
// ------------------------------------ //
// Folder collection
bool Database::InsertCollectionToFolder(
    Lock& guard, Folder& folder, const Collection& collection)
{
    if(!collection.IsInDatabase() || !folder.IsInDatabase())
        return false;

    const char str[] = "INSERT INTO folder_collection (parent, child) VALUES(?, ?);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(folder.GetID(), collection.GetID());

    statementobj.StepAll(statementinuse);

    const auto changes = sqlite3_changes(SQLiteDb);
    return changes == 1;
}

void Database::DeleteCollectionFromFolder(Folder& folder, const Collection& collection)
{

    const char str[] = "DELETE FROM folder_collection WHERE parent = ? AND child = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(folder.GetID(), collection.GetID());

    statementobj.StepAll(statementinuse);
}

std::vector<std::shared_ptr<Collection>> Database::SelectCollectionsInFolder(
    const Folder& folder, const std::string& matchingpattern /*= ""*/)
{
    GUARD_LOCK();

    const auto usePattern = !matchingpattern.empty();

    std::vector<std::shared_ptr<Collection>> result;

    const char str[] =
        "SELECT collections.* FROM folder_collection "
        "LEFT JOIN collections ON id = child "
        "WHERE parent = ?1 AND name LIKE ?2 ORDER BY (CASE WHEN name = ?3 THEN 1 "
        "WHEN name LIKE ?4 THEN 2 ELSE name END);";

    const char strNoMatch[] =
        "SELECT collections.* FROM folder_collection "
        "LEFT JOIN collections ON id = child WHERE parent = ?1 ORDER BY name;";

    PreparedStatement statementobj(SQLiteDb, usePattern ? str : strNoMatch,
        usePattern ? sizeof(str) : sizeof(strNoMatch));

    auto statementinuse = usePattern ?
                              statementobj.Setup(folder.GetID(), "%" + matchingpattern + "%",
                                  matchingpattern, matchingpattern) :
                              statementobj.Setup(folder.GetID());

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        result.push_back(_LoadCollectionFromRow(guard, statementobj));
    }

    return result;
}

bool Database::SelectCollectionIsInFolder(Lock& guard, const Collection& collection)
{

    const char str[] = "SELECT 1 FROM folder_collection WHERE child = ? LIMIT 1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID());

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return true;
    }

    return false;
}

bool Database::SelectCollectionIsInAnotherFolder(
    Lock& guard, const Folder& folder, const Collection& collection)
{
    const char str[] = "SELECT 1 FROM folder_collection WHERE child = ? AND parent != ? "
                       "LIMIT 1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID(), folder.GetID());

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        // There is
        return true;
    }

    return false;
}

std::vector<DBID> Database::SelectFoldersCollectionIsIn(const Collection& collection)
{
    GUARD_LOCK();

    std::vector<DBID> result;
    const char str[] = "SELECT parent FROM folder_collection WHERE child = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID());

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID id;
        if(statementobj.GetObjectIDFromColumn(id, 0))
            result.push_back(id);
    }

    return result;
}

void Database::DeleteCollectionFromRootIfInAnotherFolder(const Collection& collection)
{

    GUARD_LOCK();

    auto& root = *SelectRootFolder(guard);

    if(!SelectCollectionIsInAnotherFolder(guard, root, collection)) {

        return;
    }

    const char str[] = "DELETE FROM folder_collection WHERE child = ? AND parent = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID(), root.GetID());

    statementobj.StepAll(statementinuse);
}

void Database::InsertCollectionToRootIfInNone(const Collection& collection)
{

    GUARD_LOCK();

    if(SelectCollectionIsInFolder(guard, collection))
        return;

    auto& root = *SelectRootFolder(guard);

    InsertCollectionToFolder(guard, root, collection);
}

// ------------------------------------ //
// Folder folder
void Database::InsertFolderToFolder(Lock& guard, Folder& folder, const Folder& parent)
{

    const char str[] = "INSERT INTO folder_folder (parent, child) VALUES(?, ?);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(parent.GetID(), folder.GetID());

    statementobj.StepAll(statementinuse);
}

std::shared_ptr<Folder> Database::SelectFolderByNameAndParent(
    Lock& guard, const std::string& name, const Folder& parent)
{
    const char str[] =
        "SELECT virtual_folders.* FROM folder_folder "
        "LEFT JOIN virtual_folders ON id = child WHERE parent = ?1 AND name = ?2;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(parent.GetID(), name);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return _LoadFolderFromRow(guard, statementobj);
    }

    return nullptr;
}

std::vector<DBID> Database::SelectFolderParents(const Folder& folder)
{

    GUARD_LOCK();

    std::vector<DBID> result;
    const char str[] = "SELECT parent FROM folder_folder WHERE child = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(folder.GetID());

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID id;
        if(statementobj.GetObjectIDFromColumn(id, 0))
            result.push_back(id);
    }

    return result;
}

std::vector<std::shared_ptr<Folder>> Database::SelectFoldersInFolder(
    const Folder& folder, const std::string& matchingpattern /*= ""*/)
{
    GUARD_LOCK();

    std::vector<std::shared_ptr<Folder>> result;

    const auto usePattern = !matchingpattern.empty();

    const char str[] =
        "SELECT virtual_folders.* FROM folder_folder "
        "LEFT JOIN virtual_folders ON id = child "
        "WHERE parent = ?1 AND name LIKE ?2 ORDER BY (CASE WHEN name = ?3 THEN 1 "
        "WHEN name LIKE ?4 THEN 2 ELSE name END);";

    const char strNoMatch[] =
        "SELECT virtual_folders.* FROM folder_folder "
        "LEFT JOIN virtual_folders ON id = child WHERE parent = ?1 ORDER BY name;";

    PreparedStatement statementobj(SQLiteDb, usePattern ? str : strNoMatch,
        usePattern ? sizeof(str) : sizeof(strNoMatch));

    auto statementinuse = usePattern ?
                              statementobj.Setup(folder.GetID(), "%" + matchingpattern + "%",
                                  matchingpattern, matchingpattern) :
                              statementobj.Setup(folder.GetID());

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        result.push_back(_LoadFolderFromRow(guard, statementobj));
    }

    return result;
}

// ------------------------------------ //
// Tag
std::shared_ptr<Tag> Database::InsertTag(
    std::string name, std::string description, TAG_CATEGORY category, bool isprivate)
{
    GUARD_LOCK();

    const char str[] = "INSERT INTO tags (name, category, description, is_private) VALUES "
                       "(?, ?, ?, ?);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse =
        statementobj.Setup(name, static_cast<int64_t>(category), description, isprivate);

    statementobj.StepAll(statementinuse);

    return SelectTagByID(guard, sqlite3_last_insert_rowid(SQLiteDb));
}

std::shared_ptr<Tag> Database::SelectTagByID(Lock& guard, DBID id)
{

    const char str[] = "SELECT * FROM tags WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(id);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return _LoadTagFromRow(guard, statementobj);
    }

    return nullptr;
}

std::shared_ptr<Tag> Database::SelectTagByName(Lock& guard, const std::string& name)
{

    const char str[] = "SELECT * FROM tags WHERE name = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(name);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return _LoadTagFromRow(guard, statementobj);
    }

    return nullptr;
}

std::vector<std::shared_ptr<Tag>> Database::SelectTagsWildcard(
    const std::string& pattern, int64_t max /*= 50*/, bool aliases /*= true*/)
{
    GUARD_LOCK();

    std::vector<std::shared_ptr<Tag>> result;

    {
        const char str[] = "SELECT * FROM tags WHERE name LIKE ? ORDER BY name LIMIT ?;";

        PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

        auto statementinuse = statementobj.Setup("%" + pattern + "%", max);

        while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

            result.push_back(_LoadTagFromRow(guard, statementobj));
        }
    }

    // Aliases //
    {
        const char str[] = "SELECT tags.* FROM tag_aliases LEFT JOIN tags ON "
                           "tags.id = tag_aliases.meant_tag WHERE tag_aliases.name LIKE ?1 "
                           "ORDER BY tag_aliases.name LIMIT ?2;";

        PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

        // The limit guarantees at least one alias
        auto statementinuse = statementobj.Setup(
            "%" + pattern + "%", static_cast<int64_t>(1 + (max - result.size())));

        while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

            // Skip duplicates //
            DBID id;

            if(!statementobj.GetObjectIDFromColumn(id, 0))
                continue;

            bool skip = false;

            for(const auto& tag : result) {

                if(tag->GetID() == id) {

                    skip = true;
                    break;
                }
            }

            if(skip)
                continue;

            result.push_back(_LoadTagFromRow(guard, statementobj));

            if(static_cast<int64_t>(result.size()) > max)
                break;
        }
    }

    return result;
}

std::shared_ptr<Tag> Database::SelectTagByAlias(Lock& guard, const std::string& alias)
{

    const char str[] =
        "SELECT tags.* FROM tag_aliases "
        "LEFT JOIN tags ON tags.id = tag_aliases.meant_tag WHERE tag_aliases.name = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(alias);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return _LoadTagFromRow(guard, statementobj);
    }

    return nullptr;
}

std::shared_ptr<Tag> Database::SelectTagByNameOrAlias(const std::string& name)
{

    GUARD_LOCK();

    auto tag = SelectTagByName(guard, name);

    if(tag)
        return tag;

    return SelectTagByAlias(guard, name);
}

std::string Database::SelectTagSuperAlias(const std::string& name)
{

    GUARD_LOCK();

    const char str[] = "SELECT expanded FROM tag_super_aliases WHERE alias = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(name);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return statementobj.GetColumnAsString(0);
    }

    return "";
}

void Database::UpdateTag(Tag& tag)
{

    if(!tag.IsInDatabase())
        return;

    GUARD_LOCK();

    const char str[] = "UPDATE tags SET name = ?, category = ?, description = ?, "
                       "is_private = ? WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse =
        statementobj.Setup(tag.GetName(), static_cast<int64_t>(tag.GetCategory()),
            tag.GetDescription(), tag.GetIsPrivate(), tag.GetID());

    statementobj.StepAll(statementinuse);
}

bool Database::InsertTagAlias(Tag& tag, const std::string& alias)
{

    if(!tag.IsInDatabase())
        return false;

    GUARD_LOCK();

    {
        const char str[] = "SELECT * FROM tag_aliases WHERE name = ?;";

        PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

        auto statementinuse = statementobj.Setup(alias);

        if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

            return false;
        }
    }

    const char str[] = "INSERT INTO tag_aliases (name, meant_tag) VALUES (?, ?);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(alias, tag.GetID());

    statementobj.StepAll(statementinuse);
    return true;
}

void Database::DeleteTagAlias(const std::string& alias)
{

    GUARD_LOCK();

    const char str[] = "DELETE FROM tag_aliases WHERE name = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(alias);

    statementobj.StepAll(statementinuse);
}

void Database::DeleteTagAlias(const Tag& tag, const std::string& alias)
{

    GUARD_LOCK();

    const char str[] = "DELETE FROM tag_aliases WHERE name = ? AND meant_tag = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(alias, tag.GetID());

    statementobj.StepAll(statementinuse);
}

std::vector<std::string> Database::SelectTagAliases(const Tag& tag)
{

    GUARD_LOCK();

    std::vector<std::string> result;

    const char str[] = "SELECT name FROM tag_aliases WHERE meant_tag = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(tag.GetID());

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        result.push_back(statementobj.GetColumnAsString(0));
    }

    return result;
}

bool Database::InsertTagImply(Tag& tag, const Tag& implied)
{

    GUARD_LOCK();

    {
        const char str[] = "SELECT 1 FROM tag_implies WHERE primary_tag = ? AND to_apply = ?;";

        PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

        auto statementinuse = statementobj.Setup(tag.GetID(), implied.GetID());

        if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

            return false;
        }
    }

    const char str[] = "INSERT INTO tag_implies (primary_tag, to_apply) VALUES (?, ?);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(tag.GetID(), implied.GetID());

    statementobj.StepAll(statementinuse);
    return true;
}

void Database::DeleteTagImply(Tag& tag, const Tag& implied)
{

    GUARD_LOCK();

    const char str[] = "DELETE FROM tag_implies WHERE primary_tag = ? AND to_apply = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(tag.GetID(), implied.GetID());

    statementobj.StepAll(statementinuse);
}

std::vector<std::shared_ptr<Tag>> Database::SelectTagImpliesAsTag(const Tag& tag)
{

    GUARD_LOCK();

    std::vector<std::shared_ptr<Tag>> result;
    const auto tags = SelectTagImplies(guard, tag);

    for(auto id : tags) {

        auto tag = SelectTagByID(guard, id);

        if(!tag) {

            LOG_ERROR("Database: implied tag not found, id: " + Convert::ToString(id));
            continue;
        }

        result.push_back(tag);
    }

    return result;
}

std::vector<DBID> Database::SelectTagImplies(Lock& guard, const Tag& tag)
{

    std::vector<DBID> result;

    const char str[] = "SELECT to_apply FROM tag_implies WHERE primary_tag = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(tag.GetID());

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID id;

        if(statementobj.GetObjectIDFromColumn(id, 0)) {

            result.push_back(id);
        }
    }

    return result;
}

//
// AppliedTag
//
std::shared_ptr<AppliedTag> Database::SelectAppliedTagByID(Lock& guard, DBID id)
{

    const char str[] = "SELECT * FROM applied_tag WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(id);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return _LoadAppliedTagFromRow(guard, statementobj);
    }

    return nullptr;
}

std::shared_ptr<AppliedTag> Database::SelectExistingAppliedTag(
    Lock& guard, const AppliedTag& tag)
{
    DBID id = SelectExistingAppliedTagID(guard, tag);

    if(id == -1)
        return nullptr;

    return SelectAppliedTagByID(guard, id);
}

DBID Database::SelectExistingAppliedTagID(Lock& guard, const AppliedTag& tag)
{

    const char str[] = "SELECT id FROM applied_tag WHERE tag = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(tag.GetTag()->GetID());

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID id;

        if(!statementobj.GetObjectIDFromColumn(id, 0))
            continue;

        // Check are modifiers and combines the same //
        if(!CheckDoesAppliedTagModifiersMatch(guard, id, tag))
            continue;

        if(!CheckDoesAppliedTagCombinesMatch(guard, id, tag))
            continue;

        // Everything matched //
        return id;
    }

    return -1;
}

std::vector<std::shared_ptr<TagModifier>> Database::SelectAppliedTagModifiers(
    Lock& guard, const AppliedTag& appliedtag)
{
    std::vector<std::shared_ptr<TagModifier>> result;

    const char str[] = "SELECT modifier FROM applied_tag_modifier WHERE to_tag = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(appliedtag.GetID());

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID id;

        if(statementobj.GetObjectIDFromColumn(id, 0)) {

            result.push_back(SelectTagModifierByID(guard, id));
        }
    }

    return result;
}

std::tuple<std::string, std::shared_ptr<AppliedTag>> Database::SelectAppliedTagCombine(
    Lock& guard, const AppliedTag& appliedtag)
{
    const char str[] = "SELECT * FROM applied_tag_combine WHERE tag_left = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(appliedtag.GetID());

    if(statementobj.Step(statementinuse) != PreparedStatement::STEP_RESULT::ROW) {

        return std::make_tuple("", nullptr);
    }

    CheckRowID(statementobj, 1, "tag_right");
    CheckRowID(statementobj, 2, "combined_with");

    DBID id;
    if(!statementobj.GetObjectIDFromColumn(id, 1)) {

        LOG_ERROR("Database SelectAppliedTagModifier: missing tag_right id");
        return std::make_tuple("", nullptr);
    }

    return std::make_tuple(statementobj.GetColumnAsString(2), SelectAppliedTagByID(guard, id));
}

bool Database::InsertAppliedTag(Lock& guard, AppliedTag& tag)
{

    {
        const char str[] = "INSERT INTO applied_tag (tag) VALUES (?);";

        PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

        auto statementinuse = statementobj.Setup(tag.GetTag()->GetID());

        statementobj.StepAll(statementinuse);
    }

    const auto id = sqlite3_last_insert_rowid(SQLiteDb);

    tag.Adopt(id);

    std::string combinestr;
    std::shared_ptr<AppliedTag> combined;
    if(tag.GetCombinedWith(combinestr, combined)) {

        LEVIATHAN_ASSERT(
            !combinestr.empty(), "Trying to insert tag with empty combine string");

        // Insert combine //
        DBID otherid = -1;

        if(combined->GetID() != -1) {

            otherid = combined->GetID();

        } else {

            auto existingother = SelectExistingAppliedTag(guard, *combined);

            if(existingother) {

                otherid = existingother->GetID();

            } else {

                // Need to create the other side //
                if(!InsertAppliedTag(guard, *combined)) {

                    LOG_ERROR("Database: failed to create right side of combine_with tag");

                } else {

                    otherid = combined->GetID();
                }
            }
        }

        if(otherid != -1 && otherid != id) {

            // Insert it //
            const char str[] = "INSERT INTO applied_tag_combine (tag_left, tag_right, "
                               "combined_with) VALUES (?, ?, ?);";

            PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

            auto statementinuse = statementobj.Setup(id, otherid, combinestr);

            try {
                statementobj.StepAll(statementinuse);
            } catch(const InvalidSQL& e) {

                LOG_ERROR("Database: failed to insert combined with, exception: ");
                e.PrintToLog();
            }
        }
    }

    // Insert modifiers //
    for(auto& modifier : tag.GetModifiers()) {

        if(!modifier->IsInDatabase())
            continue;

        const char str[] = "INSERT INTO applied_tag_modifier (to_tag, modifier) "
                           "VALUES (?, ?);";

        PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

        auto statementinuse = statementobj.Setup(id, modifier->GetID());

        try {
            statementobj.StepAll(statementinuse);
        } catch(const InvalidSQL& e) {

            LOG_ERROR("Database: failed to insert modifier to AppliedTag, exception: ");
            e.PrintToLog();
        }
    }

    return true;
}

void Database::DeleteAppliedTagIfNotUsed(Lock& guard, AppliedTag& tag)
{

    if(SelectIsAppliedTagUsed(guard, tag.GetID()))
        return;

    // Not used, delete //
    const char str[] = "DELETE FROM applied_tag WHERE id = ?1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(tag.GetID());

    statementobj.StepAll(statementinuse);

    tag.Orphaned();
}

bool Database::SelectIsAppliedTagUsed(Lock& guard, DBID id)
{

    // Check images
    {
        const char str[] = "SELECT 1 FROM image_tag WHERE tag = ? LIMIT 1;";

        PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

        auto statementinuse = statementobj.Setup(id);

        if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

            // In use //
            return true;
        }
    }

    // Check collections
    {
        const char str[] = "SELECT 1 FROM collection_tag WHERE tag = ? LIMIT 1;";

        PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

        auto statementinuse = statementobj.Setup(id);

        if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

            // In use //
            return true;
        }
    }

    // Check tag combines
    {
        const char str[] = "SELECT 1 FROM applied_tag_combine WHERE tag_left = ?1 OR "
                           "tag_right = ?1 LIMIT 1;";

        PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

        auto statementinuse = statementobj.Setup(id);

        if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

            // In use //
            return true;
        }
    }

    return false;
}

bool Database::CheckDoesAppliedTagModifiersMatch(Lock& guard, DBID id, const AppliedTag& tag)
{

    const char str[] = "SELECT modifier FROM applied_tag_modifier WHERE to_tag = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(id);

    std::vector<DBID> modifierids;
    const auto& tagmodifiers = tag.GetModifiers();

    modifierids.reserve(tagmodifiers.size());

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID modid;

        if(!statementobj.GetObjectIDFromColumn(modid, 0))
            continue;

        // Early fail if we loaded a tag that didn't match anything in tagmodifiers //
        bool found = false;

        for(const auto& tagmod : tagmodifiers) {

            if(tagmod->GetID() == modid) {

                found = true;
                break;
            }
        }

        if(!found)
            return false;

        // Store for matching other way //
        modifierids.push_back(modid);
    }

    // Fail if modifierids and tagmodifiers don't contain the same things //
    if(modifierids.size() != tagmodifiers.size()) {

        return false;
    }

    for(const auto& tagmod : tagmodifiers) {

        DBID neededid = tagmod->GetID();

        bool found = false;

        for(const auto& dbmodifier : modifierids) {

            if(dbmodifier == neededid) {

                found = true;
                break;
            }
        }

        if(!found)
            return false;
    }

    return true;
}

bool Database::CheckDoesAppliedTagCombinesMatch(Lock& guard, DBID id, const AppliedTag& tag)
{

    // Determine id of the right side //
    DBID rightside = -1;

    std::string combinestr;
    std::shared_ptr<AppliedTag> otherside;

    if(tag.GetCombinedWith(combinestr, otherside)) {

        rightside = SelectExistingAppliedTagID(guard, *otherside);
    }

    const char str[] = "SELECT * FROM applied_tag_combine WHERE tag_left = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(id);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        // No combine used //
        if(rightside == -1) {

            // Which means that this failed //
            return false;
        }

        std::string combined_with = statementobj.GetColumnAsString(2);

        if(combined_with != combinestr) {

            // Combine doesn't match //
            return false;
        }

        DBID dbright = 0;

        statementobj.GetObjectIDFromColumn(dbright, 1);

        if(dbright != rightside) {

            // Doesn't match //
            return false;
        }

        // Everything matched //
        return true;
    }

    // Succeeded if there wasn't supposed to be a combine
    return rightside == -1;
}

DBID Database::SelectAppliedTagIDByOffset(Lock& guard, int64_t offset)
{

    const char str[] = "SELECT id FROM applied_tag ORDER BY id ASC LIMIT 1 OFFSET ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(offset);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return statementobj.GetColumnAsInt64(0);

    } else {

        LOG_WARNING("Database failed to retrieve applied_tag with offset: " +
                    Convert::ToString(offset));
    }

    return -1;
}

void Database::CombineAppliedTagDuplicate(Lock& guard, DBID first, DBID second)
{

    LEVIATHAN_ASSERT(first != second, "CombienAppliedTagDuplicate called with the same tag");

    // Update references //
    // It's also possible that the change would cause duplicates.
    // So after updating delete the rest

    // Collection
    RunSQLAsPrepared(
        guard, "UPDATE collection_tag SET tag = ?1 WHERE tag = ?2;", first, second);

    RunSQLAsPrepared(guard, "DELETE FROM collection_tag WHERE tag = ?;", second);

    // Image
    RunSQLAsPrepared(guard, "UPDATE image_tag SET tag = ?1 WHERE tag = ?2;", first, second);

    RunSQLAsPrepared(guard, "DELETE FROM image_tag WHERE tag = ?;", second);

    // combine left side
    RunSQLAsPrepared(guard,
        "UPDATE applied_tag_combine SET tag_left = ?1 "
        "WHERE tag_left = ?2;",
        first, second);

    RunSQLAsPrepared(guard, "DELETE FROM applied_tag_combine WHERE tag_left = ?;", second);

    // combine right side
    RunSQLAsPrepared(guard,
        "UPDATE applied_tag_combine SET tag_right = ?1 "
        "WHERE tag_right = ?2;",
        first, second);

    RunSQLAsPrepared(guard, "DELETE FROM applied_tag_combine WHERE tag_right = ?;", second);

    LEVIATHAN_ASSERT(!SelectIsAppliedTagUsed(guard, second),
        "CombienAppliedTagDuplicate failed to remove all references to tag");

    // And the delete it //
    const char str[] = "DELETE FROM applied_tag WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(second);

    statementobj.StepAll(statementinuse);
}


//
// TagModifier
//
std::shared_ptr<TagModifier> Database::SelectTagModifierByID(Lock& guard, DBID id)
{

    const char str[] = "SELECT * FROM tag_modifiers WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(id);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return _LoadTagModifierFromRow(guard, statementobj);
    }

    return nullptr;
}

std::shared_ptr<TagModifier> Database::SelectTagModifierByName(
    Lock& guard, const std::string& name)
{
    const char str[] = "SELECT * FROM tag_modifiers WHERE name = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(name);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return _LoadTagModifierFromRow(guard, statementobj);
    }

    return nullptr;
}

std::shared_ptr<TagModifier> Database::SelectTagModifierByAlias(
    Lock& guard, const std::string& alias)
{
    const char str[] =
        "SELECT tag_modifiers.* FROM tag_modifier_aliases "
        "LEFT JOIN tag_modifiers ON tag_modifiers.id = tag_modifier_aliases.meant_modifier "
        "WHERE tag_modifier_aliases.name = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(alias);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return _LoadTagModifierFromRow(guard, statementobj);
    }

    return nullptr;
}

std::shared_ptr<TagModifier> Database::SelectTagModifierByNameOrAlias(
    Lock& guard, const std::string& name)
{
    auto tag = SelectTagModifierByName(guard, name);

    if(tag)
        return tag;

    return SelectTagModifierByAlias(guard, name);
}

void Database::UpdateTagModifier(const TagModifier& modifier)
{

    if(!modifier.IsInDatabase())
        return;

    GUARD_LOCK();

    const char str[] = "UPDATE tag_modifiers SET name = ?, description = ?, "
                       "is_private = ? WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(modifier.GetName(), modifier.GetDescription(),
        modifier.GetIsPrivate(), modifier.GetID());

    statementobj.StepAll(statementinuse);
}

//
// TagBreakRule
//
std::shared_ptr<TagBreakRule> Database::SelectTagBreakRuleByExactPattern(
    Lock& guard, const std::string& pattern)
{
    const char str[] = "SELECT * FROM common_composite_tags WHERE tag_string = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(pattern);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return _LoadTagBreakRuleFromRow(guard, statementobj);
    }

    return nullptr;
}

std::shared_ptr<TagBreakRule> Database::SelectTagBreakRuleByStr(const std::string& searchstr)
{

    GUARD_LOCK();

    auto exact = SelectTagBreakRuleByExactPattern(guard, searchstr);

    if(exact)
        return exact;

    const char str[] = "SELECT * FROM common_composite_tags WHERE "
                       "REPLACE(tag_string, '*', '') = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(searchstr);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return _LoadTagBreakRuleFromRow(guard, statementobj);
    }

    return nullptr;
}

std::vector<std::shared_ptr<TagModifier>> Database::SelectModifiersForBreakRule(
    Lock& guard, const TagBreakRule& rule)
{
    std::vector<std::shared_ptr<TagModifier>> result;

    const char str[] = "SELECT modifier FROM composite_tag_modifiers WHERE composite = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(rule.GetID());

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID id;

        if(statementobj.GetObjectIDFromColumn(id, 0)) {

            result.push_back(SelectTagModifierByID(guard, id));
        }
    }

    return result;
}

void Database::UpdateTagBreakRule(const TagBreakRule& rule)
{

    GUARD_LOCK();
}

//
// NetGallery
//
std::vector<DBID> Database::SelectNetGalleryIDs(bool nodownloaded)
{

    GUARD_LOCK();

    std::vector<DBID> result;

    PreparedStatement statementobj(
        SQLiteDb, (nodownloaded ? "SELECT id FROM net_gallery WHERE is_downloaded = 0;" :
                                  "SELECT id FROM net_gallery;"));

    auto statementinuse = statementobj.Setup();

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID id;

        if(statementobj.GetObjectIDFromColumn(id, 0)) {

            result.push_back(id);
        }
    }

    return result;
}

std::shared_ptr<NetGallery> Database::SelectNetGalleryByID(Lock& guard, DBID id)
{

    const char str[] = "SELECT * FROM net_gallery WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(id);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return _LoadNetGalleryFromRow(guard, statementobj);
    }

    return nullptr;
}

bool Database::InsertNetGallery(Lock& guard, std::shared_ptr<NetGallery> gallery)
{

    if(gallery->IsInDatabase())
        return false;

    const char str[] =
        "INSERT INTO net_gallery (gallery_url, target_path, gallery_name, "
        "currently_scanned, is_downloaded, tags_string) VALUES (?, ?, ?, ?, ?, ?);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(gallery->GetGalleryURL(),
        gallery->GetTargetPath(), gallery->GetTargetGalleryName(),
        gallery->GetCurrentlyScanned(), gallery->GetIsDownloaded(), gallery->GetTagsString());

    statementobj.StepAll(statementinuse);

    gallery->OnAdopted(sqlite3_last_insert_rowid(SQLiteDb), *this);

    DualView::Get().QueueDBThreadFunction(
        []() { DualView::Get().GetEvents().FireEvent(CHANGED_EVENT::NET_GALLERY_CREATED); });
    return true;
}

void Database::UpdateNetGallery(NetGallery& gallery)
{

    if(!gallery.IsInDatabase())
        return;

    GUARD_LOCK();

    const char str[] =
        "UPDATE net_gallery SET gallery_url = ?, target_path = ?, "
        "gallery_name = ?, currently_scanned = ?, is_downloaded = ?, tags_string = ? "
        "WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(gallery.GetGalleryURL(), gallery.GetTargetPath(),
        gallery.GetTargetGalleryName(), gallery.GetCurrentlyScanned(),
        gallery.GetIsDownloaded(), gallery.GetTagsString(), gallery.GetID());

    statementobj.StepAll(statementinuse);
}

//
// NetFile
//
std::vector<std::shared_ptr<NetFile>> Database::SelectNetFilesFromGallery(NetGallery& gallery)
{

    GUARD_LOCK();

    std::vector<std::shared_ptr<NetFile>> result;

    PreparedStatement statementobj(SQLiteDb, "SELECT * FROM net_files WHERE "
                                             "belongs_to_gallery = ?;");

    auto statementinuse = statementobj.Setup(gallery.GetID());

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        result.push_back(_LoadNetFileFromRow(guard, statementobj));
    }

    return result;
}

std::shared_ptr<NetFile> Database::SelectNetFileByID(Lock& guard, DBID id)
{

    const char str[] = "SELECT * FROM net_files WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(id);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return _LoadNetFileFromRow(guard, statementobj);
    }

    return nullptr;
}

void Database::InsertNetFile(Lock& guard, NetFile& netfile, NetGallery& gallery)
{

    if(!gallery.IsInDatabase())
        return;

    const char str[] = "INSERT INTO net_files (file_url, page_referrer, preferred_name, "
                       "tags_string, belongs_to_gallery) VALUES (?, ?, ?, ?, ?);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(netfile.GetFileURL(), netfile.GetPageReferrer(),
        netfile.GetPreferredName(), netfile.GetTagsString(), gallery.GetID());

    statementobj.StepAll(statementinuse);
    netfile.OnAdopted(sqlite3_last_insert_rowid(SQLiteDb), *this);
}

void Database::UpdateNetFile(NetFile& netfile)
{

    GUARD_LOCK();

    const char str[] = "UPDATE net_files SET file_url = ?, referrer = ?, preferred_name = ?, "
                       "tags_string = ?, belongs_to_gallery = ? WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(netfile.GetFileURL(), netfile.GetPageReferrer(),
        netfile.GetPreferredName(), netfile.GetTagsString(), netfile.GetID());

    statementobj.StepAll(statementinuse);
}



//
// Wilcard searches for tag suggestions
//
//! \brief Returns text of all break rules that contain str
void Database::SelectTagBreakRulesByStrWildcard(
    std::vector<std::string>& breakrules, const std::string& pattern)
{
    GUARD_LOCK();

    const char str[] = "SELECT tag_string FROM common_composite_tags WHERE "
                       "REPLACE(tag_string, '*', '') LIKE ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup("%" + pattern + "%");

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        breakrules.push_back(statementobj.GetColumnAsString(0));
    }
}

void Database::SelectTagNamesWildcard(
    std::vector<std::string>& result, const std::string& pattern)
{
    GUARD_LOCK();

    const char str[] = "SELECT name FROM tags WHERE name LIKE ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup("%" + pattern + "%");

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        result.push_back(statementobj.GetColumnAsString(0));
    }
}

void Database::SelectTagAliasesWildcard(
    std::vector<std::string>& result, const std::string& pattern)
{
    GUARD_LOCK();

    const char str[] = "SELECT name FROM tag_aliases WHERE name LIKE ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup("%" + pattern + "%");

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        result.push_back(statementobj.GetColumnAsString(0));
    }
}

void Database::SelectTagModifierNamesWildcard(
    std::vector<std::string>& result, const std::string& pattern)
{
    GUARD_LOCK();

    const char str[] = "SELECT name FROM tag_modifiers WHERE name LIKE ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup("%" + pattern + "%");

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        result.push_back(statementobj.GetColumnAsString(0));
    }
}

void Database::SelectTagSuperAliasWildcard(
    std::vector<std::string>& result, const std::string& pattern)
{
    GUARD_LOCK();

    const char str[] = "SELECT alias FROM tag_super_aliases WHERE alias LIKE ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup("%" + pattern + "%");

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        result.push_back(statementobj.GetColumnAsString(0));
    }
}

// ------------------------------------ //
// Database maintainance functions
void Database::CombineAllPossibleAppliedTags(Lock& guard)
{

    int64_t count = 0;

    {
        const char str[] = "SELECT COUNT(*) FROM applied_tag;";

        PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

        auto statementinuse = statementobj.Setup();

        if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

            count = statementobj.GetColumnAsInt64(0);
        }
    }

    LOG_INFO("Database: Maintainance combining all applied_tags that are the same. "
             "Modifier count: " +
             Convert::ToString(count));

    // For super speed check against only other tags that have the same primary tag
    const char str[] = "SELECT id FROM applied_tag WHERE tag = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    for(int64_t i = 0; i < count; ++i) {

        DBID currentid = SelectAppliedTagIDByOffset(guard, i);

        if(currentid < 0)
            continue;

        auto currenttag = SelectAppliedTagByID(guard, currentid);

        auto statementinuse = statementobj.Setup(currenttag->GetTag()->GetID());

        while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

            DBID otherid = -1;
            if(!statementobj.GetObjectIDFromColumn(otherid, 0))
                continue;

            // Don't compare with self
            if(currentid == otherid)
                continue;

            // Primary tags should match already //

            // Then check modifiers and combines
            if(!CheckDoesAppliedTagModifiersMatch(guard, otherid, *currenttag))
                continue;

            if(!CheckDoesAppliedTagCombinesMatch(guard, otherid, *currenttag))
                continue;

            LOG_INFO("Database: found matching AppliedTags, " + Convert::ToString(currentid) +
                     " == " + Convert::ToString(otherid));

            CombineAppliedTagDuplicate(guard, currentid, otherid);

            // count is now smaller //
            // But because we are looping by offset ordered by id we should be able to
            // continue without adjusting i
            --count;
        }
    }

    // Verify that count is still right, there shouldn't be anything at offset count
    DBID verifyisinvalid = SelectAppliedTagIDByOffset(guard, count);

    LEVIATHAN_ASSERT(verifyisinvalid == -1, "Combine AppliedTag decreasing count has "
                                            "resulted in wrong number");

    LOG_INFO(
        "Database: maintainance shrunk applied_tag count to: " + Convert::ToString(count));

    // Finish off by deleting duplicate combines
    RunSQLAsPrepared(guard, "DELETE FROM applied_tag_combine WHERE rowid NOT IN "
                            "(SELECT min(rowid) FROM applied_tag_combine GROUP BY "
                            "tag_left, tag_right, combined_with);");

    LOG_INFO("Database: Maintainance for combining all applied_tags finished.");
}

int64_t Database::CountAppliedTags()
{

    GUARD_LOCK();

    const char str[] = "SELECT COUNT(*) FROM applied_tag;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup();

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        return statementobj.GetColumnAsInt64(0);
    }

    return 0;
}


// ------------------------------------ //
// Row parsing functions
std::shared_ptr<NetFile> Database::_LoadNetFileFromRow(
    Lock& guard, PreparedStatement& statement)
{
    CheckRowID(statement, 0, "id");

    DBID id;
    if(!statement.GetObjectIDFromColumn(id, 0)) {

        LOG_ERROR("Object id column is invalid");
        return nullptr;
    }

    return std::make_shared<NetFile>(*this, guard, statement, id);
}

std::shared_ptr<NetGallery> Database::_LoadNetGalleryFromRow(
    Lock& guard, PreparedStatement& statement)
{
    CheckRowID(statement, 0, "id");

    DBID id;
    if(!statement.GetObjectIDFromColumn(id, 0)) {

        LOG_ERROR("Object id column is invalid");
        return nullptr;
    }

    auto loaded = LoadedNetGalleries.GetIfLoaded(id);

    if(loaded)
        return loaded;

    loaded = std::make_shared<NetGallery>(*this, guard, statement, id);

    LoadedNetGalleries.OnLoad(loaded);
    return loaded;
}

std::shared_ptr<TagBreakRule> Database::_LoadTagBreakRuleFromRow(
    Lock& guard, PreparedStatement& statement)
{
    CheckRowID(statement, 0, "id");

    DBID id;
    if(!statement.GetObjectIDFromColumn(id, 0)) {

        LOG_ERROR("Object id column is invalid");
        return nullptr;
    }

    return std::make_shared<TagBreakRule>(*this, guard, statement, id);
}

std::shared_ptr<AppliedTag> Database::_LoadAppliedTagFromRow(
    Lock& guard, PreparedStatement& statement)
{
    CheckRowID(statement, 0, "id");

    DBID id;
    if(!statement.GetObjectIDFromColumn(id, 0)) {

        LOG_ERROR("Object id column is invalid");
        return nullptr;
    }

    return std::make_shared<AppliedTag>(*this, guard, statement, id);
}

std::shared_ptr<TagModifier> Database::_LoadTagModifierFromRow(
    Lock& guard, PreparedStatement& statement)
{
    CheckRowID(statement, 0, "id");

    DBID id;
    if(!statement.GetObjectIDFromColumn(id, 0)) {

        LOG_ERROR("Object id column is invalid");
        return nullptr;
    }

    return std::make_shared<TagModifier>(*this, guard, statement, id);
}

std::shared_ptr<Tag> Database::_LoadTagFromRow(Lock& guard, PreparedStatement& statement)
{

    CheckRowID(statement, 0, "id");

    DBID id;
    if(!statement.GetObjectIDFromColumn(id, 0)) {

        LOG_ERROR("Object id column is invalid");
        return nullptr;
    }

    auto loaded = LoadedTags.GetIfLoaded(id);

    if(loaded)
        return loaded;

    loaded = std::make_shared<Tag>(*this, guard, statement, id);

    LoadedTags.OnLoad(loaded);
    return loaded;
}

std::shared_ptr<Collection> Database::_LoadCollectionFromRow(
    Lock& guard, PreparedStatement& statement)
{
    CheckRowID(statement, 0, "id");

    DBID id;
    if(!statement.GetObjectIDFromColumn(id, 0)) {

        LOG_ERROR("Object id column is invalid");
        return nullptr;
    }

    auto loaded = LoadedCollections.GetIfLoaded(id);

    if(loaded)
        return loaded;

    loaded = std::make_shared<Collection>(*this, guard, statement, id);

    LoadedCollections.OnLoad(loaded);
    return loaded;
}

std::shared_ptr<Image> Database::_LoadImageFromRow(Lock& guard, PreparedStatement& statement)
{
    CheckRowID(statement, 0, "id");

    DBID id;
    if(!statement.GetObjectIDFromColumn(id, 0)) {

        LOG_ERROR("Object id column is invalid");
        return nullptr;
    }

    auto loaded = LoadedImages.GetIfLoaded(id);

    if(loaded)
        return loaded;

    loaded = Image::Create(*this, guard, statement, id);

    LoadedImages.OnLoad(loaded);
    return loaded;
}

std::shared_ptr<Folder> Database::_LoadFolderFromRow(Lock& guard, PreparedStatement& statement)
{
    CheckRowID(statement, 0, "id");

    DBID id;
    if(!statement.GetObjectIDFromColumn(id, 0)) {

        LOG_ERROR("Object id column is invalid");
        return nullptr;
    }

    auto loaded = LoadedFolders.GetIfLoaded(id);

    if(loaded)
        return loaded;

    loaded = std::make_shared<Folder>(*this, guard, statement, id);

    LoadedFolders.OnLoad(loaded);
    return loaded;
}

// ------------------------------------ //
void Database::ThrowCurrentSqlError(Lock& guard)
{

    ThrowErrorFromDB(SQLiteDb);
}
// ------------------------------------ //
bool Database::_VerifyLoadedVersion(Lock& guard, int fileversion)
{

    if(fileversion == DATABASE_CURRENT_VERSION)
        return true;

    // Fail if trying to load a newer version
    if(fileversion > DATABASE_CURRENT_VERSION) {

        LOG_ERROR("Trying to load a database that is newer than program's version");
        return false;
    }

    // Update the database //
    int updateversion = fileversion;

    LOG_INFO("Database: updating from version " + Convert::ToString(updateversion) +
             " to version " + Convert::ToString(DATABASE_CURRENT_VERSION));

    while(updateversion != DATABASE_CURRENT_VERSION) {

        if(!_UpdateDatabase(guard, updateversion)) {

            LOG_ERROR("Database update failed, database file version is unsupported");
            return false;
        }

        // Get new version //
        if(!SelectDatabaseVersion(guard, updateversion)) {

            LOG_ERROR("Database failed to retrieve new version after update");
            return false;
        }
    }

    return true;
}

bool Database::_UpdateDatabase(Lock& guard, const int oldversion)
{

    if(oldversion < 14) {

        LOG_ERROR("Migrations from version 13 and older aren't copied to DualView++ "
                  "and thus not possible to load a database that old");

        return false;
    }

    LEVIATHAN_ASSERT(boost::filesystem::exists(DatabaseFile),
        "UpdateDatabase called when DatabaseFile doesn't exist");

    // Create a backup //
    int suffix = 1;
    std::string targetfile;

    do {
        targetfile = DatabaseFile + "." + Convert::ToString(suffix) + ".bak";
        ++suffix;

    } while(boost::filesystem::exists(targetfile));

    boost::filesystem::copy(DatabaseFile, targetfile);

    LOG_INFO("Database: running update from version " + Convert::ToString(oldversion));

    switch(oldversion) {
    case 14: {
        _RunSQL(guard,
            LoadResourceCopy("/com/boostslair/dualviewpp/resources/sql/migration_14_15.sql"));
        _SetCurrentDatabaseVersion(guard, 15);
        return true;
    }
    case 15: {
        // Delete all invalid AppliedTags
        RunSQLAsPrepared(guard, "DELETE FROM applied_tag WHERE tag = -1;");

        LOG_WARNING("During this update sqlite won't run in synchronous mode");
        _RunSQL(guard, "PRAGMA synchronous = OFF; PRAGMA journal_mode = MEMORY;");

        // This makes sure all appliedtags are unique, and combines are fine
        // which is required for the new version
        try {
            CombineAllPossibleAppliedTags(guard);

        } catch(...) {

            // Save progress //
            sqlite3_db_cacheflush(SQLiteDb);
            throw;
        }

        _RunSQL(guard, "PRAGMA synchronous = ON; PRAGMA journal_mode = DELETE;");

        _RunSQL(guard,
            LoadResourceCopy("/com/boostslair/dualviewpp/resources/sql/migration_15_16.sql"));
        _SetCurrentDatabaseVersion(guard, 16);


        return true;
    }
    case 16: {
        _RunSQL(guard,
            LoadResourceCopy("/com/boostslair/dualviewpp/resources/sql/migration_16_17.sql"));
        _SetCurrentDatabaseVersion(guard, 17);
        return true;
    }
    case 17: {
        // There was a bug where online image tags weren't applied to the images so we need
        // to apply those
        _UpdateApplyDownloadTagStrings(guard);
        _SetCurrentDatabaseVersion(guard, 18);
        return true;
    }
    case 18: {
        _RunSQL(guard,
            LoadResourceCopy("/com/boostslair/dualviewpp/resources/sql/migration_18_19.sql"));
        _SetCurrentDatabaseVersion(guard, 19);
        return true;
    }
    default: {
        LOG_ERROR("Unknown database version to update from: " + Convert::ToString(oldversion));
        return false;
    }
    }
}

void Database::_SetCurrentDatabaseVersion(Lock& guard, int newversion)
{

    if(sqlite3_exec(SQLiteDb,
           ("UPDATE version SET number = " + Convert::ToString(newversion) + ";").c_str(),
           nullptr, nullptr, nullptr) != SQLITE_OK) {
        ThrowCurrentSqlError(guard);
    }
}

void Database::_UpdateApplyDownloadTagStrings(Lock& guard)
{

    const char str[] =
        "SELECT pictures.id, net_files.tags_string FROM net_files "
        "INNER JOIN pictures ON net_files.file_url = pictures.from_file WHERE "
        "net_files.tags_string IS NOT NULL AND LENGTH(net_files.tags_string) > 0;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup();

    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW) {

        DBID imgid;

        if(!statementobj.GetObjectIDFromColumn(imgid, 0)) {

            LOG_ERROR("Invalid DB update id received");
            continue;
        }

        const auto tags = statementobj.GetColumnAsString(1);

        if(tags.empty()) {

            LOG_WARNING("DB update skipping applying empty tag string");
            continue;
        }

        // Load the image //
        auto image = SelectImageByID(guard, imgid);

        if(!image) {

            LOG_ERROR("DB update didn't find image a tag string should be applied to");
            continue;
        }

        // Apply it //
        std::vector<std::string> tagparts;
        Leviathan::StringOperations::CutString<std::string>(tags, ";", tagparts);

        for(auto& line : tagparts) {

            if(line.empty())
                continue;

            guard.unlock();

            std::shared_ptr<AppliedTag> tag;

            try {

                tag = DualView::Get().ParseTagFromString(line);

            } catch(const Leviathan::Exception& e) {

                LOG_ERROR("DB Update applying tag failed. Invalid tag: " + line);
                continue;
            }

            guard.lock();

            if(!tag)
                continue;

            InsertImageTag(guard, image, *tag);

            LOG_INFO("Applied tag " + tag->ToAccurateString() + " to " + image->GetName());
        }

        LOG_INFO("Applied DB download tag string to image id: " + std::to_string(imgid));
    }
}
// ------------------------------------ //
void Database::_CreateTableStructure(Lock& guard)
{
    _RunSQL(guard, "BEGIN TRANSACTION;");

    _RunSQL(
        guard, LoadResourceCopy("/com/boostslair/dualviewpp/resources/sql/maintables.sql"));

    _RunSQL(guard,
        LoadResourceCopy("/com/boostslair/dualviewpp/resources/sql/defaulttablevalues.sql"));

    _RunSQL(
        guard, LoadResourceCopy("/com/boostslair/dualviewpp/resources/sql/defaulttags.sql"));

    // Default collections //
    InsertCollection(guard, "Uncategorized", false);
    InsertCollection(guard, "PrivateRandom", true);
    InsertCollection(guard, "Backgrounds", false);

    // Insert version last //
    _RunSQL(guard, "INSERT INTO version (number) VALUES (" +
                       Convert::ToString(DATABASE_CURRENT_VERSION) + ");");

    _RunSQL(guard, "COMMIT TRANSACTION;");
}

void Database::_RunSQL(Lock& guard, const std::string& sql)
{
    auto result = sqlite3_exec(SQLiteDb, sql.c_str(), nullptr, nullptr, nullptr);

    if(SQLITE_OK != result) {

        ThrowErrorFromDB(SQLiteDb, result);
    }
}

void Database::PrintResultingRows(Lock& guard, const std::string& str)
{

    PreparedStatement statementobj(SQLiteDb, str);

    auto statementinuse = statementobj.Setup();

    LOG_INFO("SQL result from: \"" + str + "\"");
    statementobj.StepAndPrettyPrint(statementinuse);
}
// ------------------------------------ //
int Database::SqliteExecGrabResult(
    void* user, int columns, char** columnsastext, char** columnname)
{
    auto* grabber = reinterpret_cast<GrabResultHolder*>(user);

    if(grabber->MaxRows > 0 && (grabber->Rows.size() >= grabber->MaxRows))
        return 1;

    GrabResultHolder::Row row;

    for(int i = 0; i < columns; ++i) {

        row.ColumnValues.push_back(columnsastext[i] ? columnsastext[i] : "");
        row.ColumnNames.push_back(columnname[i] ? columnname[i] : "");
    }

    grabber->Rows.push_back(row);
    return 0;
}
// ------------------------------------ //
std::string Database::EscapeSql(std::string str)
{

    str = Leviathan::StringOperations::Replace<std::string>(str, "\n", " ");
    str = Leviathan::StringOperations::Replace<std::string>(str, "\r\n", " ");
    str = Leviathan::StringOperations::Replace<std::string>(str, "\"\"", "\"");
    str = Leviathan::StringOperations::Replace<std::string>(str, "\"", "\"\"");

    return str;
}

// Transaction stuff
void Database::BeginTransaction(Lock& guard)
{

    RunSQLAsPrepared(guard, "BEGIN TRANSACTION;");
}

void Database::CommitTransaction(Lock& guard)
{

    RunSQLAsPrepared(guard, "COMMIT TRANSACTION;");
}

// ------------------------------------ //
// DoDBTransaction
DoDBTransaction::DoDBTransaction(Database& db, Lock& dblock) : DB(db), Locked(dblock)
{

    DB.BeginTransaction(Locked);
}

DoDBTransaction::~DoDBTransaction()
{

    DB.CommitTransaction(Locked);
}
