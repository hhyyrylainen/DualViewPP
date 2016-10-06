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

    CurlWrapper urlencoder;
    char* curlEncoded = curl_easy_escape(urlencoder.Get(), dbfile.c_str(), dbfile.size()); 
    
    dbfile = std::string(curlEncoded);

    curl_free(curlEncoded);

    // If begins with : add a ./ to the beginning
    if(dbfile[0] == ':')
        dbfile = "./" + dbfile;
    
    // Add the file uri specifier
    dbfile = "file:" + dbfile;
    
    const auto result = sqlite3_open_v2(dbfile.c_str(), &SQLiteDb,
        SQLITE_OPEN_URI | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
        ""
    );
    
    
}

Database::Database(bool tests){

    LEVIATHAN_ASSERT(tests, "Database test version not constructed with true");

    const auto result = sqlite3_open_v2(":memory:", &SQLiteDb,
        SQLITE_OPEN_URI,
        ""
    );

    
}
// ------------------------------------ //

