#pragma once

#include "Database.h"
#include "PreparedStatement.h"

#include "TimeHelpers.h"

#include <boost/filesystem.hpp>

namespace DV {

//! \brief In-memory database for testing purposes
class TestDatabase : public Database {
public:
    TestDatabase() : Database(true) {}

    std::shared_ptr<Image> InsertTestImage(const std::string& file, const std::string& hash)
    {
        GUARD_LOCK();
        return InsertTestImage(guard, file, hash);
    }

    std::shared_ptr<Image> InsertTestImage(
        Database::LockT& guard, const std::string& file, const std::string& hash)
    {
        const char str[] =
            "INSERT INTO pictures (relative_path, name, extension, file_hash, "
            "width, height, add_date, last_view) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

        PreparedStatement statementobj(SQLiteDb, str, sizeof(str));

        auto statementinuse =
            statementobj.Setup(file, boost::filesystem::path(file).filename().string(),
                boost::filesystem::path(file).extension().string(), hash, 50, 50,
                TimeHelpers::format8601(date::make_zoned(date::current_zone(),
                    std::chrono::time_point_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now()))),
                TimeHelpers::format8601(date::make_zoned(date::current_zone(),
                    std::chrono::time_point_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now()))));

        try {

            statementobj.StepAll(statementinuse);

        } catch(const InvalidSQL& e) {

            throw;
        }

        return SelectImageByHash(guard, hash);
    }

    //! Very unsafe
    void Run(const std::string& str)
    {
        GUARD_LOCK();

        _RunSQL(guard, str);
    }

    template<typename... TBindTypes>
    void Run(const std::string& str, TBindTypes&&... valuestobind)
    {
        GUARD_LOCK();

        PreparedStatement statementobj(SQLiteDb, str);

        auto statementinuse = statementobj.Setup(std::forward<TBindTypes>(valuestobind)...);

        statementobj.StepAll(statementinuse);
    }

    //! Prints the applied tag table
    void PrintAppliedTagTable()
    {
        GUARD_LOCK();

        PrintResultingRows(guard, GetDB(), "SELECT * FROM applied_tag");
    }

    sqlite3* GetDB()
    {
        return SQLiteDb;
    }
};

} // namespace DV
