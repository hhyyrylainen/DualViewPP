// ------------------------------------ //
#include "Database.h"
#include "Common.h"
#include "Exceptions.h"


#include "generated/maintables.sql.h"
#include "generated/defaulttablevalues.sql.h"
#include "generated/defaulttags.sql.h"

#include "core/CurlWrapper.h"

#include <sqlite3.h>

#include <boost/filesystem.hpp>

#include <thread>

using namespace DV;
// ------------------------------------ //

namespace DV{
//! \brief Helper class for working with prepared statements
//! \note The sqlite object needs to be locked whenever using this class
class PreparedStatement{
public:

    //! \brief Helper class to make sure statements are initialized before Step is called
    //!
    //! Must be created each time a statement is to be used
    class SetupStatementForUse{
    public:
        template<typename... TBindTypes>
            SetupStatementForUse(PreparedStatement& statement,
                const TBindTypes&... valuestobind) :
                Statement(statement)
        {
            Statement.Reset();
            Statement.BindAll(valuestobind...);
        }

        ~SetupStatementForUse(){

            Statement.Reset();
        }

        PreparedStatement& Statement;
    };

    enum class STEP_RESULT {

        ROW,
        COMPLETED
    };

    PreparedStatement(sqlite3* sqlite, const char* str, size_t length) : DB(sqlite){

        const char* uncompiled;

        const auto result = sqlite3_prepare_v2(DB, str, length, &Statement, &uncompiled);
        if(result != SQLITE_OK)
        {
            sqlite3_finalize(Statement);
            Database::ThrowErrorFromDB(DB, result, "compiling statement: '"
                + std::string(str) + "'");
        }

        // Check was there stuff not compiled
        if(uncompiled < str + sizeof(str)){

            UncompiledPart = std::string(uncompiled);
            LOG_WARNING("SQL statement not processed completely: " + UncompiledPart);
        }
    }

    ~PreparedStatement(){

        sqlite3_finalize(Statement);
    }

    //! Call after doing steps to reset for next use
    void Reset(){

        CurrentBindIndex = 1;
        sqlite3_reset(Statement);
    }

    //! \brief Steps the statement forwards, automatically throws if fails
    //! \param isprepared A created object that makes sure this object is setup correctly
    STEP_RESULT Step(const SetupStatementForUse &isprepared){

        const auto result = sqlite3_step(Statement);

        if(result == SQLITE_DONE){

            // Finished //
            return STEP_RESULT::COMPLETED;
        }

        if(result == SQLITE_BUSY){

            LOG_WARNING("SQL statement: database is busy, retrying...");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return Step(isprepared);
        }

        if(result == SQLITE_ROW){

            // A row can be read //
            return STEP_RESULT::ROW;
        }

        // An error! //
        LOG_ERROR("An error occured in an sql statement, code: " + Convert::ToString(result));
        Database::ThrowErrorFromDB(DB, result);
        
        // Execution never reaches this, as the above function will always throw
        return STEP_RESULT::COMPLETED;
    }

    // Row functions
    // \note These should only be called when Step has returned a row
    
    auto GetColumnCount() const{

        return sqlite3_column_count(Statement);
    }

    //! This will return one of:
    //! SQLITE_INTEGER
    //! SQLITE_FLOAT 
    //! SQLITE_BLOB 
    //! SQLITE_NULL 
    //! SQLITE_TEXT
    auto GetColumnType(int column) const{

        return sqlite3_column_type(Statement, column);
    }

    bool IsColumnNull(int column) const{

        return GetColumnType(column) == SQLITE_NULL;
    }

    auto GetColumnName(int column){
        
        const char* name = sqlite3_column_name(Statement, column);
        return name ? std::string(name) : std::string("unknown");
    }

    // Warning these functions will do inplace conversions and the types
    // will be inconsistent after these calls. You should first call GetColumnType
    // and then one of these get as functions based on the type
    
    auto GetColumnAsInt(int column){

        return sqlite3_column_int(Statement, column);
    }

    auto GetColumnAsInt64(int column){

        return sqlite3_column_int64(Statement, column);
    }

    auto GetColumnAsString(int column){

        return sqlite3_column_int64(Statement, column);
    }

    //! \brief Sets id to be the value from column
    //! \returns True if column is valid and not null
    bool GetObjectIDFromColumn(DBID &id, int column = 0){

        if(column >= GetColumnCount())
            return false;

        if(IsColumnNull(column) || GetColumnType(column) != SQLITE_INTEGER)
            return false;

        id = GetColumnAsInt64(column);
        return true;
    }

    // Binding functions for parameters
    template<typename TParam, typename = std::enable_if<false>>
        void SetBindWithType(int index, const TParam &value)
    {
        static_assert(sizeof(TParam) == -1, "SetBindWithType non-specialized version called");
    }


    template<typename TParamType>
        PreparedStatement& Bind(const TParamType &value)
    {
        SetBindWithType(CurrentBindIndex, value);
        ++CurrentBindIndex;
        return *this;
    }

    PreparedStatement& BindAll(){

        return *this;
    }

    template<typename TFirstParam, typename... TRestOfTypes>
        PreparedStatement& BindAll(const TFirstParam &value,
            const TRestOfTypes&... othervalues)
    {
        Bind(value);
        return BindAll(othervalues...);
    }

    //! \brief Throws if returncode isn't SQLITE_OK
    inline void CheckBindSuccess(int returncode, int index){

        if(returncode == SQLITE_OK)
            return;

        auto* desc = sqlite3_errstr(returncode);

        throw InvalidSQL("Binding argument at index " + Convert::ToString(index) + " failed",
            returncode, desc ? desc : "no description");        
    }

    sqlite3* DB;
    sqlite3_stmt* Statement;

    //! Contains the left over parts of the statement, empty if only one statement was passed
    std::string UncompiledPart;
    
    int CurrentBindIndex = 1;
    
};

template<>
    void PreparedStatement::SetBindWithType(int index, const int &value)
{
    CheckBindSuccess(sqlite3_bind_int(Statement, index, value), index);
}

template<>
    void PreparedStatement::SetBindWithType(int index, const std::string &value)
{
    CheckBindSuccess(sqlite3_bind_text(Statement, index, value.c_str(), value.size(),
            SQLITE_TRANSIENT), index);
}

template<>
    void PreparedStatement::SetBindWithType(int index, const std::nullptr_t &value)
{
    CheckBindSuccess(sqlite3_bind_null(Statement, index), index);
}

}

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

std::shared_ptr<Image> Database::SelectImageByHash(const std::string &hash) const{

    GUARD_LOCK();
    
    const char str[] = "SELECT id FROM pictures WHERE file_hash = ?1;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = PreparedStatement::SetupStatementForUse(statementobj,
        hash);
    
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
size_t Database::CountExistingTags(){

    GUARD_LOCK();

    const char str[] = "SELECT COUNT(*) FROM tags;";

    PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

    auto statementinuse = PreparedStatement::SetupStatementForUse(statementobj);
    
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
void Database::ThrowCurrentSqlError(Lock &guard){

    ThrowErrorFromDB(SQLiteDb);
}

void Database::ThrowErrorFromDB(sqlite3* sqlite, int code /*= 0*/,
    const std::string &extramessage)
{

    if(code == 0)
        code = sqlite3_errcode(sqlite);

    auto* msg = sqlite3_errmsg(sqlite);
    auto* desc = sqlite3_errstr(code);

    const auto* msgStr = msg ? msg : "no message";

    fprintf(stdout, "Can't open database: %s\n", sqlite3_errmsg(sqlite));

    throw InvalidSQL(!extramessage.empty() ?
        (msgStr + std::string(", While: ") +extramessage ) : msgStr,
        code,
        desc ? desc : "no description");
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
// ------------------------------------ //
// InvalidSQL
InvalidSQL::InvalidSQL(const std::string &message,
    int32_t code, const std::string &codedescription) noexcept :
    ErrorCode(code)
{
    FinalMessage = "[SQL EXCEPTION] ([" + Convert::ToString(ErrorCode) + "] "+
        codedescription + "): " + message;
}
const char* InvalidSQL::what() const noexcept{

    return FinalMessage.c_str();
}

void InvalidSQL::PrintToLog() const noexcept{

    LOG_WRITE(FinalMessage);
}

