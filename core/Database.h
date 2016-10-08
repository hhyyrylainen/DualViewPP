#pragma once

#include "Common/ThreadSafe.h"

#include <string>
#include <vector>

#include <thread>

// Forward declare sqlite //
struct sqlite3;

namespace DV{

//! \brief An exception thrown when sql errors occur
class InvalidSQL : public std::exception{
public:

    InvalidSQL(const std::string &message,
        int32_t code, const std::string &codedescription) noexcept;
    InvalidSQL(const InvalidSQL &e) = default;
    
    ~InvalidSQL() = default;
    
    InvalidSQL& operator=(const InvalidSQL &other) = default;
    
    const char* what() const noexcept override;
    void PrintToLog() const noexcept;

    inline operator bool() const noexcept{

        return ErrorCode != 0;
    }

protected:
    
    std::string FinalMessage;
    int32_t ErrorCode;

};

class CurlWrapper;
class Image;

//! \brief The version number of the database
constexpr auto DATABASE_CURRENT_VERSION = 14;

//! \brief All database manipulation happens through this class
//!
//! There should be only one database object at a time. It is contained in DualView
class Database : public Leviathan::ThreadSafe{

    //! \brief Holds data in SqliteExecGrabResult
    struct GrabResultHolder{

        struct Row{

            std::vector<std::string> ColumnValues;
            std::vector<std::string> ColumnNames;
        };

        size_t MaxRows = 0;
        std::vector<Row> Rows;
    };

    //! \brief Helper class for creating prepared statements
    
    
public:

    //! \brief Normal database creation, uses the specified file
    Database(std::string dbfile);

    //! \brief Creates an in-memory database for testing, the tests parameter must be
    //! true
    Database(bool tests);
    
    //! \brief Closes the database safely
    ~Database();

    //! \brief Must be called before using the database. Setups all the tables
    //! \exception Leviathan::InvalidState if the database setup fails
    //! \note Must be called on the thread that 
    void Init();

    //! \brief Returns an Image matching the hash, or null
    std::shared_ptr<Image> SelectImageByHash(const std::string &hash) const;
    
    //! \brief Selects the database version
    //! \returns True if succeeded, false if np version exists.
    bool SelectDatabaseVersion(Lock &guard, int &result);
    

    // Statistics functions //
    size_t CountExistingTags();

    //! \brief Helper callback for standard operations
    //! \warning The user parameter has to be a pointer to Database::GrabResultHolder
    static int SqliteExecGrabResult(void* user, int columns, char** columnsastext,
        char** columnname);

    //! \brief Throws an InvalidSQL exceptions from sqlite current error
    static void ThrowErrorFromDB(sqlite3* sqlite, int code = 0,
        const std::string &extramessage = "");

private:

    //! \brief Throws an InvalidSQL exception, filling it with values from the database
    //! connection
    void ThrowCurrentSqlError(Lock &guard);

    //! \brief Creates default tables and also calls _InsertDefaultTags
    void _CreateTableStructure(Lock &guard);
    
    //! \brief Inserts default inbuilt tags
    void _InsertDefaultTags(Lock &guard);

    //! \brief Verifies that the specified version is compatible with the current version
    bool _VerifyLoadedVersion(Lock &guard, int fileversion);

    //! \brief Called if a loaded database version is older than DATABASE_CURRENT_VERSION
    //! \param oldversion The version from which the update is done, will contain the new
    //! version after this returns. Can be called again to update to the next version
    //! \returns True if succeeded, false if update is not supported
    bool _UpdateDatabase(Lock &guard, int &oldversion);

    //! \brief Sets the database version. Should only be called from _UpdateDatabase
    void _SetCurrentDatabaseVersion(Lock &guard, int newversion);
    
protected:


    sqlite3* SQLiteDb = nullptr;

    //! Used for backups before potentially dangerous operations
    std::string DatabaseFile;
};

}

