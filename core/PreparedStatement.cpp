// ------------------------------------ //
#include "PreparedStatement.h"

#include "Database.h"

// ------------------------------------ //
// In a namespace to make template specializations more simple
namespace DV{

PreparedStatement::PreparedStatement(sqlite3* sqlite, const char* str, size_t length) :
    DB(sqlite)
{

    const char* uncompiled;

    const auto result = sqlite3_prepare_v2(DB, str, length, &Statement, &uncompiled);
    if(result != SQLITE_OK)
    {
        sqlite3_finalize(Statement);
        ThrowErrorFromDB(DB, result, "compiling statement: '"
            + std::string(str) + "'");
    }

    // Check was there stuff not compiled
    if(uncompiled < str + sizeof(str)){

        UncompiledPart = std::string(uncompiled);
        LOG_WARNING("SQL statement not processed completely: " + UncompiledPart);
    }
}

PreparedStatement::PreparedStatement(sqlite3* sqlite, const std::string &str) :
    PreparedStatement(sqlite, str.c_str(), str.size())
{
    
}

template<>
    void PreparedStatement::SetBindWithType(int index, const int &value)
{
    CheckBindSuccess(sqlite3_bind_int(Statement, index, value), index);
}

template<>
    void PreparedStatement::SetBindWithType(int index, const int64_t &value)
{
    CheckBindSuccess(sqlite3_bind_int64(Statement, index, value), index);
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

template<>
    void PreparedStatement::SetBindWithType(int index, const bool &value)
{
    CheckBindSuccess(sqlite3_bind_int(Statement, index, value ? 1 : 0), index);
}


}
