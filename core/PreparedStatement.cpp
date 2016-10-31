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
    void PreparedStatement::SetBindWithType(int index, const long long int &value)
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

// ------------------------------------ //
void PreparedStatement::StepAndPrettyPrint(const SetupStatementForUse &isprepared){

    int rowcount = 0;
    bool headerprinted = false;
    
    while(Step(isprepared) != STEP_RESULT::COMPLETED){

        if(!headerprinted){

            PrettyPrintColumnNames();
            headerprinted = true;
        }

        PrintRowValues();
        ++rowcount;
    }

    if(!headerprinted){

        // Empty result //
        LOG_WRITE("SQL: RESULT HAS 0 ROWS");
        
    } else {

        std::string str = "| TOTAL ROWS: " + Convert::ToString(rowcount);
        str.resize(80, '*');
        LOG_WRITE(str);
    }
}

void PreparedStatement::PrettyPrintColumnNames(){

    std::string str = "*SQL RESULT SET";
    str.resize(80, '_');
    LOG_WRITE(str);

    str = "| ";

    for(int i = 0; i < GetColumnCount(); ++i){

        str += GetColumnName(i) + " | ";
    }
            
    LOG_WRITE(str);
    
    str = "|";
    str.resize(80, '-');
    LOG_WRITE(str);
}

void PreparedStatement::PrintRowValues(){

    std::string str = "| ";

    for(int i = 0; i < GetColumnCount(); ++i){

        str += GetColumnAsString(i) + " | ";
    }
            
    LOG_WRITE(str);
    str = "|";
    str.resize(80, '-');
    LOG_WRITE(str);
}

}
