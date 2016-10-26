// ------------------------------------ //
#include "Database.h"
#include "Common.h"
#include "Exceptions.h"

#include "PreparedStatement.h"

#include "core/resources/Image.h"
#include "core/resources/Collection.h"
#include "core/resources/Tags.h"
#include "core/resources/Folder.h"

#include "core/TimeHelpers.h"

#include "generated/maintables.sql.h"
#include "generated/defaulttablevalues.sql.h"
#include "generated/defaulttags.sql.h"

#include "core/CurlWrapper.h"

#include "Common/StringOperations.h"

#include <sqlite3.h>

#include <boost/filesystem.hpp>

#include <thread>

using namespace DV;
// ------------------------------------ //
Database::Database(std::string dbfile) : DatabaseFile(dbfile){

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
        SQLITE_OPEN_URI | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
        SQLITE_OPEN_NOMUTEX,
        nullptr
    );

    if(!SQLiteDb){

        throw Leviathan::InvalidState("failed to allocate memory for sqlite database");
    }

    if(result != SQLITE_OK){

        const std::string errormessage(sqlite3_errmsg(SQLiteDb));

        sqlite3_close(SQLiteDb);
        SQLiteDb = nullptr;
        LOG_ERROR("Sqlite failed to open database '" + dbfile + "' errorcode: " +
            Convert::ToString(result) + " message: " + errormessage);
        throw Leviathan::InvalidState("failed to open sqlite database");
    }
}

Database::Database(bool tests){

    LEVIATHAN_ASSERT(tests, "Database test version not constructed with true");

    const auto result = sqlite3_open(":memory:", &SQLiteDb);

    if(result != SQLITE_OK || SQLiteDb == nullptr){

        sqlite3_close(SQLiteDb);
        SQLiteDb = nullptr;
        throw Leviathan::InvalidState("failed to open memory sqlite database");
    }
}

Database::~Database(){

    GUARD_LOCK();
    // No operations can be in progress, as we are locked
    // But if there were that would be an error in DualView not properly
    // shutting everything down
    

    // Stop all active operations //
    

    // Release all prepared objects //


    if(SQLiteDb){

        while(sqlite3_close(SQLiteDb) != SQLITE_OK){

            LOG_WARNING("Database waiting for sqlite3 resources to be released, "
                "database cannot be closed yet");
        }

        SQLiteDb = nullptr;
    }
}
// ------------------------------------ //
void Database::Init(){

    GUARD_LOCK();
    
    if(SQLITE_OK != sqlite3_exec(SQLiteDb,
            "PRAGMA foreign_keys = ON; PRAGMA recursive_triggers = ON",
            nullptr, nullptr, nullptr))
    {
        throw Leviathan::InvalidState("Failed to enable foreign keys");
    }

    // Verify foreign keys are on //
    {
        GrabResultHolder grab;
        sqlite3_exec(SQLiteDb, "PRAGMA foreign_keys; PRAGMA recursive_triggers;",
            &Database::SqliteExecGrabResult,
            &grab, nullptr);

        if(grab.Rows.size() != 2 || grab.Rows[0].ColumnValues[0] != "1" ||
            grab.Rows[1].ColumnValues[0] != "1")
        {
            throw Leviathan::InvalidState("Foreign keys didn't get enabled");
        }
    }

    // Verify database version and setup tables if they don't exist //
    int fileVersion = -1;

    if(!SelectDatabaseVersion(guard, fileVersion)){

        // Database is newly created //
        _CreateTableStructure(guard);
        
    } else {

        // Check that the version is compatible, upgrade if needed //
        if(!_VerifyLoadedVersion(guard, fileVersion)){

            throw Leviathan::InvalidState("Database file is unsupported version");
        }
    }
    
    // Setup statements //
    
}

void Database::PurgeInactiveCache(){

    GUARD_LOCK();

    LoadedCollections.Purge();
    LoadedImages.Purge();
    LoadedFolders.Purge();
    LoadedTags.Purge();
}
// ------------------------------------ //
bool Database::SelectDatabaseVersion(Lock &guard, int &result){

    GrabResultHolder grab;
    sqlite3_exec(SQLiteDb, "SELECT number FROM version;",
        &Database::SqliteExecGrabResult,
        &grab, nullptr);

    if(grab.Rows.size() != 1 || grab.Rows[0].ColumnValues.empty())
    {
        // There is no version //
        result = -1;
        return false;
    }

    result = Convert::StringTo<int>(grab.Rows[0].ColumnValues[0]);
    return true;
}
// ------------------------------------ //
// Image
void Database::InsertImage(Image &image){

    LEVIATHAN_ASSERT(image.IsReady(), "InsertImage: image not ready");

    GUARD_LOCK();
    
    const char str[] = "INSERT INTO pictures (relative_path, width, height, name, extension, "
        "add_date, last_view, is_private, from_file, file_hash) VALUES "
        "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(image.GetResourcePath(), image.GetWidth(),
        image.GetHeight(), image.GetName(), image.GetExtension(), image.GetAddDateStr(),
        image.GetLastViewStr(), image.GetIsPrivate(), image.GetFromFile(), image.GetHash());

    statementobj.StepAll(statementinuse);

    const DBID id = SelectImageIDByHash(guard, image.GetHash());
    
    image.OnAdopted(id, *this);
}

bool Database::UpdateImage(const Image &image){

    return false;
}

bool Database::DeleteImage(Image &image){

    return false;
}

DBID Database::SelectImageIDByHash(Lock &guard, const std::string &hash){

    const char str[] = "SELECT id FROM pictures WHERE file_hash = ?1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(hash);
    
    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        DBID id;
        if(statementobj.GetObjectIDFromColumn(id, 0))
            return id;
    }
    
    return -1;
}

std::shared_ptr<Image> Database::SelectImageByHash(Lock &guard, const std::string &hash){

    const char str[] = "SELECT * FROM pictures WHERE file_hash = ?1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(hash);
    
    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        return _LoadImageFromRow(guard, statementobj);
    }
    
    return nullptr;
}

std::shared_ptr<Image> Database::SelectImageByID(Lock &guard, DBID id){

    const char str[] = "SELECT * FROM pictures WHERE id = ?1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(id);
    
    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        return _LoadImageFromRow(guard, statementobj);
    }
    
    return nullptr;
}

std::shared_ptr<TagCollection> Database::LoadImageTags(const Image &image){

    if(!image.IsInDatabase())
        return nullptr;
    
    DEBUG_BREAK;
    //auto tags = std::make_shared<DatabaseTagCollection>();
    
    
    return nullptr;
    //return tags;
}

// ------------------------------------ //
// Collection
std::shared_ptr<Collection> Database::InsertCollection(Lock &guard, const std::string &name,
    bool isprivate)
{
    const char str[] = "INSERT INTO collections (name, is_private, "
        "add_date, modify_date, last_view) VALUES (?, ?, ?, ?, ?);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    const auto currentTime = TimeHelpers::FormatCurrentTimeAs8601();
    auto statementinuse = statementobj.Setup(name, isprivate,
        currentTime, currentTime, currentTime);

    try{

        statementobj.StepAll(statementinuse);

    } catch(const InvalidSQL &e){

        LOG_WARNING("Failed to InsertCollection: ");
        e.PrintToLog();
        return nullptr;
    }
    
    auto created = SelectCollectionByName(guard, name);

    // Add it to the root folder //
    if(!InsertCollectionToFolder(guard, *SelectRootFolder(guard), *created)){

        LOG_ERROR("Failed to add a new Collection to the root folder");
    }
    
    return created;
}

bool Database::UpdateCollection(const Collection &collection){

    return false;
}

bool Database::DeleteCollection(Collection &collection){

    return false;
}

std::shared_ptr<Collection> Database::SelectCollectionByID(DBID id){

    GUARD_LOCK();

    const char str[] = "SELECT * FROM collections WHERE id = ?1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(id);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        // Found a value //
        return _LoadCollectionFromRow(guard, statementobj);
    }
    
    return nullptr;
}

std::shared_ptr<Collection> Database::SelectCollectionByName(Lock &guard,
    const std::string &name)
{
     const char str[] = "SELECT * FROM collections WHERE name = ?1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(name);

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        // Found a value //
        return _LoadCollectionFromRow(guard, statementobj);
    }
    
    return nullptr;
}

int64_t Database::SelectCollectionLargestShowOrder(const Collection &collection){

    if(!collection.IsInDatabase())
        return 0;
    
    GUARD_LOCK();

    const char str[] = "SELECT show_order FROM collection_image WHERE collection = ?1 "
        "ORDER BY show_order DESC LIMIT 1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID());

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        return statementobj.GetColumnAsInt64(0);
    }
    
    return 0;
}

int64_t Database::SelectCollectionImageCount(const Collection &collection){

    if(!collection.IsInDatabase())
        return 0;
    
    GUARD_LOCK();

    const char str[] = "SELECT COUNT(*) FROM collection_image WHERE collection = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID());

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        return statementobj.GetColumnAsInt64(0);
    }
    
    return 0;
}

// ------------------------------------ //
// Collection image 
bool Database::InsertImageToCollection(Collection &collection, Image &image,
    int64_t showorder)
{
    if(!collection.IsInDatabase() || !image.IsInDatabase())
        return false;
    
    GUARD_LOCK();

    const char str[] = "INSERT INTO collection_image (collection, image, show_order) VALUES "
        "(?1, ?2, ?3);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID(), image.GetID(), showorder);

    statementobj.StepAll(statementinuse);

    const auto changes = sqlite3_changes(SQLiteDb);

    LEVIATHAN_ASSERT(changes <= 1, "InsertImageToCollection changed more than one row");

    return changes == 1;
}

bool Database::DeleteImageFromCollection(Collection &collection, Image &image){

    if(!collection.IsInDatabase() || !image.IsInDatabase())
        return false;
    
    GUARD_LOCK();

    const char str[] = "DELETE FROM collection_image WHERE collection = ?1 AND image = ?2;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID(), image.GetID());

    statementobj.StepAll(statementinuse);

    const auto changes = sqlite3_changes(SQLiteDb);

    LEVIATHAN_ASSERT(changes <= 1, "InsertImageToCollection changed more than one row");

    return changes == 1;
}

int64_t Database::SelectImageShowOrderInCollection(Collection &collection, Image &image){

    if(!collection.IsInDatabase() || !image.IsInDatabase())
        return -1;

    GUARD_LOCK();

    const char str[] = "SELECT show_order FROM collection_image WHERE collection = ? AND "
        "image = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID(), image.GetID());

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        return statementobj.GetColumnAsInt64(0);
    }

    return -1;
}

std::shared_ptr<Image> Database::SelectCollectionPreviewImage(const Collection &collection){

    GUARD_LOCK();

    const char str[] = "SELECT preview_image FROM collections WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID());

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        DBID preview;
        
        if(statementobj.GetObjectIDFromColumn(preview, 0)){

            // It was set //
            return SelectImageByID(guard, preview); 
        }
    }

    // There wasn't a specifically set preview image
    return SelectFirstImageInCollection(guard, collection);
}

std::shared_ptr<Image> Database::SelectFirstImageInCollection(Lock &guard,
    const Collection &collection)
{
    const char str[] = "SELECT image FROM collection_image WHERE collection = ? "
        "ORDER BY show_order ASC LIMIT 1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID());

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        DBID id;

        if(statementobj.GetObjectIDFromColumn(id, 0))
            return SelectImageByID(guard, id);
    }

    return nullptr;
}
    
// ------------------------------------ //
size_t Database::CountExistingTags(){

    GUARD_LOCK();

    const char str[] = "SELECT COUNT(*) FROM tags;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup();
    
    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        return statementobj.GetColumnAsInt64(0);
    }
    
    return 0;
}
// ------------------------------------ //
// Folder
std::shared_ptr<Folder> Database::SelectRootFolder(Lock &guard){

    const char str[] = "SELECT * FROM virtual_folders WHERE id = 1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup();
    
    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        return _LoadFolderFromRow(guard, statementobj);
    }

    LEVIATHAN_ASSERT(false, "Root folder is missing from the database");
    return nullptr;
}

std::shared_ptr<Folder> Database::SelectFolderByID(Lock &guard, DBID id){

    const char str[] = "SELECT * FROM virtual_folders WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(id);
    
    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        return _LoadFolderFromRow(guard, statementobj);
    }

    return nullptr;
}

std::shared_ptr<Folder> Database::InsertFolder(const std::string &name, bool isprivate,
    const Folder &parent)
{
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

bool Database::UpdateFolder(Folder &folder){

    return false;
}
// ------------------------------------ //
// Folder collection
bool Database::InsertCollectionToFolder(Lock &guard, Folder &folder, Collection &collection){

    if(!collection.IsInDatabase() || !folder.IsInDatabase())
        return false;
    
    const char str[] = "INSERT INTO folder_collection (parent, child) VALUES(?, ?);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(folder.GetID(), collection.GetID());

    statementobj.StepAll(statementinuse);
    
    const auto changes = sqlite3_changes(SQLiteDb);
    return changes == 1; 
}

std::vector<std::shared_ptr<Collection>> Database::SelectCollectionsInFolder(
    const Folder &folder, const std::string &matchingpattern /*= ""*/)
{
    GUARD_LOCK();

    const auto usePattern = !matchingpattern.empty();
    
    std::vector<std::shared_ptr<Collection>> result;

    const char str[] = "SELECT collections.* FROM folder_collection "
        "LEFT JOIN collections ON id = child "
        "WHERE parent = ?1 AND name LIKE ?2 ORDER BY (CASE WHEN name = ?3 THEN 1 "
        "WHEN name LIKE ?4 THEN 2 ELSE name END);";

    const char strNoMatch[] = "SELECT collections.* FROM folder_collection "
        "LEFT JOIN collections ON id = child WHERE parent = ?1 ORDER BY name;";

    PreparedStatement statementobj(SQLiteDb, usePattern ? str : strNoMatch,
        usePattern ? sizeof(str) : sizeof(strNoMatch));

    auto statementinuse = usePattern ? statementobj.Setup(folder.GetID(),
        "%" + matchingpattern + "%", matchingpattern, matchingpattern) :
        statementobj.Setup(folder.GetID()); 
    
    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        result.push_back(_LoadCollectionFromRow(guard, statementobj));
    }
    
    return result;
}

bool Database::SelectCollectionIsInAnotherFolder(Lock &guard, const Folder &folder,
    const Collection &collection)
{
    const char str[] = "SELECT 1 FROM folder_collection WHERE child = ? AND parent != ? "
        "LIMIT 1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID(), folder.GetID());

    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        // There is
        return true;
    }

    return false;
}

void Database::DeleteCollectionFromRootIfInAnotherFolder(const Collection &collection){

    GUARD_LOCK();

    auto& root = *SelectRootFolder(guard);

    if(!SelectCollectionIsInAnotherFolder(guard, root, collection)){

        return;
    }

    const char str[] = "DELETE FROM folder_collection WHERE child = ? AND parent = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(collection.GetID(), root.GetID());

    statementobj.StepAll(statementinuse);
}

// ------------------------------------ //
// Folder folder
void Database::InsertFolderToFolder(Lock &guard, Folder &folder, const Folder &parent){

    const char str[] = "INSERT INTO folder_folder (parent, child) VALUES(?, ?);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(parent.GetID(), folder.GetID());

    statementobj.StepAll(statementinuse);
}

std::shared_ptr<Folder> Database::SelectFolderByNameAndParent(Lock &guard,
    const std::string &name, const Folder &parent)
{
    const char str[] = "SELECT virtual_folders.* FROM folder_folder "
        "LEFT JOIN virtual_folders ON id = child WHERE parent = ?1 AND name = ?2;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(parent.GetID(), name); 
    
    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        return _LoadFolderFromRow(guard, statementobj);
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<Folder>> Database::SelectFoldersInFolder(const Folder &folder,
    const std::string &matchingpattern /*= ""*/)
{
    GUARD_LOCK();
    
    std::vector<std::shared_ptr<Folder>> result;

    const auto usePattern = !matchingpattern.empty();
    
    const char str[] = "SELECT virtual_folders.* FROM folder_folder "
        "LEFT JOIN virtual_folders ON id = child "
        "WHERE parent = ?1 AND name LIKE ?2 ORDER BY (CASE WHEN name = ?3 THEN 1 "
        "WHEN name LIKE ?4 THEN 2 ELSE name END);";

    const char strNoMatch[] = "SELECT virtual_folders.* FROM folder_folder "
        "LEFT JOIN virtual_folders ON id = child WHERE parent = ?1 ORDER BY name;";

    PreparedStatement statementobj(SQLiteDb, usePattern ? str : strNoMatch,
        usePattern ? sizeof(str) : sizeof(strNoMatch));

    auto statementinuse = usePattern ? statementobj.Setup(folder.GetID(),
        "%" + matchingpattern + "%", matchingpattern, matchingpattern) :
        statementobj.Setup(folder.GetID()); 
    
    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        result.push_back(_LoadFolderFromRow(guard, statementobj));
    }
    
    return result;
}

// ------------------------------------ //
// Tag
std::shared_ptr<Tag> Database::SelectTagByID(Lock &guard, DBID id){

    const char str[] = "SELECT * FROM tags WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(id); 
    
    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        return _LoadTagFromRow(guard, statementobj);
    }
    
    return nullptr;
}

std::shared_ptr<Tag> Database::SelectTagByName(Lock &guard, const std::string &name){

    const char str[] = "SELECT * FROM tags WHERE name = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(name); 
    
    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        return _LoadTagFromRow(guard, statementobj);
    }
    
    return nullptr;
}

void Database::UpdateTag(Tag &tag){

    if(!tag.IsInDatabase())
        return;

    GUARD_LOCK();

    const char str[] = "UPDATE tags SET name = ?, category = ?, description = ?, "
        "is_private = ? WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(tag.GetName(),
        static_cast<int64_t>(tag.GetCategory()), tag.GetDescription(), tag.GetIsPrivate(),
        tag.GetID()); 
    
    statementobj.StepAll(statementinuse);
}

bool Database::InsertTagAlias(Tag &tag, const std::string &alias){

    if(!tag.IsInDatabase())
        return false;
    
    GUARD_LOCK();

    {
        const char str[] = "SELECT * FROM tag_aliases WHERE name = ?;";

        PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

        auto statementinuse = statementobj.Setup(alias); 
    
        if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

            return false;
        }
    }

    const char str[] = "INSERT INTO tag_aliases (name, meant_tag) VALUES (?, ?);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(alias, tag.GetID()); 
    
    statementobj.StepAll(statementinuse);
    return true;
}

void Database::DeleteTagAlias(const std::string &alias){

    GUARD_LOCK();

    const char str[] = "DELETE FROM tag_aliases WHERE name = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(alias); 
    
    statementobj.StepAll(statementinuse);
}

void Database::DeleteTagAlias(const Tag &tag, const std::string &alias){

    GUARD_LOCK();

    const char str[] = "DELETE FROM tag_aliases WHERE name = ? AND meant_tag = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(alias, tag.GetID()); 
    
    statementobj.StepAll(statementinuse);
}

std::vector<std::shared_ptr<Tag>> Database::SelectTagImpliesAsTag(const Tag &tag){

    GUARD_LOCK();

    std::vector<std::shared_ptr<Tag>> result;
    const auto tags = SelectTagImplies(guard, tag);

    for(auto id : tags){

        auto tag = SelectTagByID(guard, id);

        if(!tag){

            LOG_ERROR("Database: implied tag not found, id: " + Convert::ToString(id));
            continue;
        }

        result.push_back(tag);
    }

    return result;
}

std::vector<DBID> Database::SelectTagImplies(Lock &guard, const Tag &tag){

    std::vector<DBID> result;

    const char str[] = "SELECT to_apply FROM tag_aliases WHERE primary_tag = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(tag.GetID()); 
    
    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        DBID id;

        if(statementobj.GetObjectIDFromColumn(id, 0)){

            result.push_back(id);
        }
    }

    return result;
}

//
// AppliedTag
//
std::shared_ptr<AppliedTag> Database::SelectAppliedTagByID(Lock &guard, DBID id){

    const char str[] = "SELECT * FROM applied_tag WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(id); 
    
    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        return _LoadAppliedTagFromRow(guard, statementobj);
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<TagModifier>> Database::SelectAppliedTagModifiers(Lock &guard,
    const AppliedTag &appliedtag)
{
    std::vector<std::shared_ptr<TagModifier>> result;

    const char str[] = "SELECT modifier FROM applied_tag_modifier WHERE to_tag = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(appliedtag.GetID()); 
    
    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        DBID id;

        if(statementobj.GetObjectIDFromColumn(id, 0)){

            result.push_back(SelectTagModifierByID(guard, id));
        }
    }

    return result;
}

std::tuple<std::string, std::shared_ptr<AppliedTag>> Database::SelectAppliedTagCombine(
    Lock &guard, const AppliedTag &appliedtag)
{
    const char str[] = "SELECT * FROM applied_tag_combine WHERE tag_left = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(appliedtag.GetID()); 
    
    if(statementobj.Step(statementinuse) != PreparedStatement::STEP_RESULT::ROW){

        return std::make_tuple("", nullptr);
    }

    CheckRowID(statementobj, 1, "tag_right");

    DBID id;
    if(statementobj.GetObjectIDFromColumn(id, 1)){

        LOG_ERROR("Database SelectAppliedTagModifier: missing tag_right id");
        return std::make_tuple("", nullptr);
    }

    return std::make_tuple(statementobj.GetColumnAsString(3), SelectAppliedTagByID(guard, id));
}

//
// TagModifier
//
std::shared_ptr<TagModifier> Database::SelectTagModifierByID(Lock &guard, DBID id){

    const char str[] = "SELECT * FROM tag_modifiers WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(id); 
    
    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        return _LoadTagModifierFromRow(guard, statementobj);
    }
    
    return nullptr;
}

std::shared_ptr<TagModifier> Database::SelectTagModifierByName(Lock &guard,
    const std::string &name)
{
    const char str[] = "SELECT * FROM tag_modifiers WHERE name = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(name); 
    
    if(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        return _LoadTagModifierFromRow(guard, statementobj);
    }
    
    return nullptr;
}

void Database::UpdateTagModifier(const TagModifier &modifier){
    
    if(!modifier.IsInDatabase())
        return;

    GUARD_LOCK();

    const char str[] = "UPDATE tag_modifiers SET name = ?, description = ?, "
        "is_private = ? WHERE id = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(modifier.GetName(),
        modifier.GetDescription(), modifier.GetIsPrivate(),
        modifier.GetID()); 
    
    statementobj.StepAll(statementinuse);
}

//
// TagBreakRule
//
std::vector<std::shared_ptr<TagModifier>> Database::SelectModifiersForBreakRule(Lock &guard,
    const TagBreakRule &rule)
{
    std::vector<std::shared_ptr<TagModifier>> result;

    const char str[] = "SELECT modifier FROM composite_tag_modifiers WHERE composite = ?;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(rule.GetID()); 
    
    while(statementobj.Step(statementinuse) == PreparedStatement::STEP_RESULT::ROW){

        DBID id;

        if(statementobj.GetObjectIDFromColumn(id, 0)){

            result.push_back(SelectTagModifierByID(guard, id));
        }
    }

    return result;
}
    

// ------------------------------------ //
// Row parsing functions
std::shared_ptr<AppliedTag> Database::_LoadAppliedTagFromRow(Lock &guard, 
    PreparedStatement &statement)
{
    CheckRowID(statement, 0, "id");

    DBID id;
    if(!statement.GetObjectIDFromColumn(id, 0)){

        LOG_ERROR("Object id column is invalid");
        return nullptr;
    }
    
    return std::make_shared<AppliedTag>(*this, guard, statement, id);
}

std::shared_ptr<TagModifier> Database::_LoadTagModifierFromRow(Lock &guard, 
    PreparedStatement &statement)
{
    CheckRowID(statement, 0, "id");

    DBID id;
    if(!statement.GetObjectIDFromColumn(id, 0)){

        LOG_ERROR("Object id column is invalid");
        return nullptr;
    }
    
    return std::make_shared<TagModifier>(*this, guard, statement, id);
}

std::shared_ptr<Tag> Database::_LoadTagFromRow(Lock &guard, PreparedStatement &statement){

    CheckRowID(statement, 0, "id");

    DBID id;
    if(!statement.GetObjectIDFromColumn(id, 0)){

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

std::shared_ptr<Collection> Database::_LoadCollectionFromRow(Lock &guard,
    PreparedStatement &statement)
{
    CheckRowID(statement, 0, "id");

    DBID id;
    if(!statement.GetObjectIDFromColumn(id, 0)){

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

std::shared_ptr<Image> Database::_LoadImageFromRow(Lock &guard,
    PreparedStatement &statement)
{
    CheckRowID(statement, 0, "id");

    DBID id;
    if(!statement.GetObjectIDFromColumn(id, 0)){

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

std::shared_ptr<Folder> Database::_LoadFolderFromRow(Lock &guard,
    PreparedStatement &statement)
{
    CheckRowID(statement, 0, "id");

    DBID id;
    if(!statement.GetObjectIDFromColumn(id, 0)){

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
void Database::ThrowCurrentSqlError(Lock &guard){

    ThrowErrorFromDB(SQLiteDb);
}
// ------------------------------------ //
bool Database::_VerifyLoadedVersion(Lock &guard, int fileversion){

    if(fileversion == DATABASE_CURRENT_VERSION)
        return true;

    // Fail if trying to load a newer version
    if(fileversion > DATABASE_CURRENT_VERSION){

        LOG_ERROR("Trying to load a database that is newer than program's version");
        return false;
    }

    // Update the database //
    int updateversion = fileversion;

    while(updateversion != DATABASE_CURRENT_VERSION){

        if(!_UpdateDatabase(guard, updateversion)){

            LOG_ERROR("Database update failed, database file version is unsupported");
            return false;
        }
    }

    return true;
}

bool Database::_UpdateDatabase(Lock &guard, int &oldversion){

    if(oldversion < 14){

        LOG_ERROR("Migrations from version 13 and older aren't copied to DualView++ "
            "and it's not possible to load a database that old");
        
        return false;
    }

    LEVIATHAN_ASSERT(boost::filesystem::exists(DatabaseFile),
        "UpdateDatabase called when DatabaseFile doesn't exist");
    
    // Create a backup //
    int suffix = 1;
    std::string targetfile;

    do
    {
        targetfile = DatabaseFile + "." + Convert::ToString(suffix) + ".bak";
        ++suffix;

    } while (boost::filesystem::exists(targetfile));

    boost::filesystem::copy(DatabaseFile, targetfile);
    
    switch(oldversion){
        

    default:
    {
        LOG_ERROR("Unknown database version to update from: " + Convert::ToString(oldversion));
        return false;
    }
    }

    return true;
}

void Database::_SetCurrentDatabaseVersion(Lock &guard, int newversion){
    
    if(sqlite3_exec(SQLiteDb,
        ("UPDATE version SET number = " + Convert::ToString(newversion) + ";").c_str(),
            nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        ThrowCurrentSqlError(guard);
    }
}
// ------------------------------------ //
void Database::_CreateTableStructure(Lock &guard){

    LOG_INFO("Initializing new database");
    
    if(sqlite3_exec(SQLiteDb, STR_MAINTABLES_SQL,
            nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        ThrowCurrentSqlError(guard);
    }

    if(sqlite3_exec(SQLiteDb, STR_DEFAULTTABLEVALUES_SQL,
            nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        ThrowCurrentSqlError(guard);
    }

    _InsertDefaultTags(guard);
    
    // Insert version last //
    if(sqlite3_exec(SQLiteDb, ("INSERT INTO version(number) VALUES(" +
                Convert::ToString(DATABASE_CURRENT_VERSION) + ");").c_str(),
            nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        ThrowCurrentSqlError(guard);
    }
}
    
void Database::_InsertDefaultTags(Lock &guard){

    if(sqlite3_exec(SQLiteDb, STR_DEFAULTTAGS_SQL,
            nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        ThrowCurrentSqlError(guard);
    }

    // Default collections //
    InsertCollection(guard, "Uncategorized", false);
    InsertCollection(guard, "PrivateRandom", true);
    InsertCollection(guard, "Backgrounds", false);
}
// ------------------------------------ //
int Database::SqliteExecGrabResult(void* user, int columns, char** columnsastext,
    char** columnname)
{
    auto* grabber = reinterpret_cast<GrabResultHolder*>(user);

    if(grabber->MaxRows > 0 && (grabber->Rows.size() >= grabber->MaxRows))
        return 1;

    GrabResultHolder::Row row;

    for(int i = 0; i < columns; ++i){

        row.ColumnValues.push_back(columnsastext[i] ? columnsastext[i] : "");
        row.ColumnNames.push_back(columnname[i] ? columnname[i] : "");
    }
    
    grabber->Rows.push_back(row);
    return 0;
}
// ------------------------------------ //
std::string Database::EscapeSql(std::string str){

    str = Leviathan::StringOperations::Replace<std::string>(str, "\n", " ");
    str = Leviathan::StringOperations::Replace<std::string>(str, "\r\n", " ");
    str = Leviathan::StringOperations::Replace<std::string>(str, "\"\"", "\"");
    str = Leviathan::StringOperations::Replace<std::string>(str, "\"", "\"\"");

    return str;
}
