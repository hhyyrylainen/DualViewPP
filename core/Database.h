#pragma once

#include <string>

// Forward declare sqlite //
struct sqlite3;

namespace DV{

class CurlWrapper;

//! \brief All database manipulation happens through this class
//!
//! There should be only one database object at a time. It is contained in DualView
class Database{
public:

    //! \brief Normal database creation, uses the specified file
    Database(std::string dbfile);

    //! \brief Creates an in-memory database for testing, the tests parameter must be
    //! true
    Database(bool tests);
    
    
    
protected:


    sqlite3* SQLiteDb = nullptr;
    
private:

    //Could be used to force all db accesses to be from a specific thread
    //std::thread::id AllowedThread;
};

}

