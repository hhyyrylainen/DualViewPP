// ------------------------------------ //
#include "Database.h"
#include "Common.h"
#include "Exceptions.h"

#include "core/CurlWrapper.h"

#include <sqlite3.h>

using namespace DV;
// ------------------------------------ //

Database::Database(std::string dbfile){

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

    // If begins with : add a ./ to the beginning
    if(dbfile[0] == ':')
        dbfile = "./" + dbfile;
    
    // Add the file uri specifier
    dbfile = "file:" + dbfile;

    const auto result = sqlite3_open_v2("dualview.sqlite", &SQLiteDb,
        SQLITE_OPEN_URI | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
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
    
}



// ------------------------------------ //
size_t Database::CountExistingTags(){

    return 1;
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
