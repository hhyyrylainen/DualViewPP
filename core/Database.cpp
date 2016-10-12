// ------------------------------------ //
#include "Database.h"
#include "Common.h"
#include "Exceptions.h"

#include "PreparedStatement.h"

#include "core/resources/Image.h"
#include "core/resources/Collection.h"
#include "core/resources/Tags.h"

#include "generated/maintables.sql.h"
#include "generated/defaulttablevalues.sql.h"
#include "generated/defaulttags.sql.h"

#include "core/CurlWrapper.h"

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


}

bool Database::UpdateImage(const Image &image){

    return false;
}

bool Database::DeleteImage(Image &image){

    return false;
}

std::shared_ptr<Image> Database::SelectImageByHash(const std::string &hash){

    GUARD_LOCK();
    
    const char str[] = "SELECT id FROM pictures WHERE file_hash = ?1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(hash);
    
    while(statementobj.Step(statementinuse) != PreparedStatement::STEP_RESULT::COMPLETED){

        DBID id;
        if(statementobj.GetObjectIDFromColumn(id)){

            // Got an object //
            LOG_FATAL("TODO: load object");
            //return;
        }
    }
    
    return nullptr;
}

// ------------------------------------ //
// Collection
std::shared_ptr<Collection> Database::InsertCollection(const std::string &name,
    bool isprivate)
{
    GUARD_LOCK();

    const char str[] = "INSERT INTO collections (name, is_private) VALUES (?1, ?2);";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup(name, isprivate);

    try{

        statementobj.StepAll(statementinuse);

    } catch(const InvalidSQL &e){

        LOG_WARNING("Failed to InsertCollection: ");
        e.PrintToLog();
        return nullptr;
    }
    
    return SelectCollectionByName(guard, name);
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
    
// ------------------------------------ //
size_t Database::CountExistingTags(){

    GUARD_LOCK();

    const char str[] = "SELECT COUNT(*) FROM tags;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = statementobj.Setup();
    
    size_t count = 0;
    
    while(statementobj.Step(statementinuse) != PreparedStatement::STEP_RESULT::COMPLETED){

        // Handle a row //
        if(statementobj.GetColumnCount() < 1){

            continue;
        }

        count = statementobj.GetColumnAsInt64(0);
    }
    
    return count;
}
// ------------------------------------ //
// Row parsing functions
std::shared_ptr<Collection> Database::_LoadCollectionFromRow(Lock &guard,
    PreparedStatement &statement)
{
    CheckRowID(statement, 0, "id");

    const auto id = statement.GetColumnAsInt64(0);
    auto loaded = LoadedCollections.GetIfLoaded(id);

    if(loaded)
        return loaded;

    loaded = std::make_shared<Collection>(*this, guard, statement, id);

    LoadedCollections.OnLoad(loaded);
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

