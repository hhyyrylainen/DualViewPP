#pragma once

#include "core/Database.h"
#include "core/PreparedStatement.h"

#include "core/TimeHelpers.h"

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
            "width, height, add_date, last_view) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

        PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

        auto statementinuse = statementobj.Setup(file,
            boost::filesystem::path(file).filename().string(),
            boost::filesystem::path(file).extension().string(),
            hash, 50, 50,
            TimeHelpers::format8601(date::make_zoned(date::current_zone(),
                    std::chrono::time_point_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now()))),
            TimeHelpers::format8601(date::make_zoned(date::current_zone(),
                    std::chrono::time_point_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now())))
        );

        try{

            statementobj.StepAll(statementinuse);

        } catch(const InvalidSQL &e){

            throw;
        }
    
        return SelectImageByHash(guard, hash);
    }

    //! Prints the applied tag table
    void PrintAppliedTagTable(){

        GUARD_LOCK();

        PrintResultingRows(guard, "SELECT * FROM applied_tag");
    }
    

};

}
