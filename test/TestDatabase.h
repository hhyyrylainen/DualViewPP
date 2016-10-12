#pragma once

#include "core/Database.h"
#include "core/PreparedStatement.h"

#include <boost/filesystem.hpp>

namespace DV{

//! \brief In-memory database for testing purposes
class TestDatabase : public Database{
public:

    TestDatabase() : Database(true){

        
    }

    std::shared_ptr<Image> InsertTestImage(const std::string &file, const std::string &hash){

        GUARD_LOCK();

        const char str[] = "INSERT INTO pictures (relative_path, name, extension, file_hash, "
            "width, height) VALUES (?, ?, ?, ?, ?, ?);";

        PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

        auto statementinuse = statementobj.Setup(file,
            boost::filesystem::path(file).filename().string(),
            boost::filesystem::path(file).extension().string(),
            hash, 50, 50);

        try{

            statementobj.StepAll(statementinuse);

        } catch(const InvalidSQL &e){

            LOG_WARNING("Failed to Test insert image: ");
            e.PrintToLog();
            return nullptr;
        }
    
        return SelectImageByHash(guard, hash);
    }
    

};

}
