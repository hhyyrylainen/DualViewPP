#pragma once

#include <string>
#include <vector>

// Forward declare sqlite //
struct sqlite3;

namespace DV{

class CurlWrapper;

//! \brief All database manipulation happens through this class
//!
//! There should be only one database object at a time. It is contained in DualView
class Database{

    //! \brief Holds data in SqliteExecGrabResult
    struct GrabResultHolder{

        struct Row{

            std::vector<std::string> ColumnValues;
            std::vector<std::string> ColumnNames;
        };

        size_t MaxRows = 0;
        std::vector<Row> Rows;
    };
    
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
    void Init();


    // Statistics functions //
    size_t CountExistingTags();

    //! \brief Helper callback for standard operations
    //! \warning The user parameter has to be a pointer to Database::GrabResultHolder
    static int SqliteExecGrabResult(void* user, int columns, char** columnsastext,
        char** columnname);
    
protected:


    sqlite3* SQLiteDb = nullptr;
    
private:

    //Could be used to force all db accesses to be from a specific thread
    //std::thread::id AllowedThread;
};

}

