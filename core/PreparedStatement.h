#pragma once

#include "Common.h"
#include "SQLHelpers.h"

#include <sqlite3.h>
#include <cstring>

#include <thread>

namespace DV{

//! \brief Helper class for working with prepared statements
//! \note The sqlite object needs to be locked whenever using this class
class PreparedStatement{
public:

    //! \brief Helper class to make sure statements are initialized before Step is called
    //!
    //! Must be created each time a statement is to be used
    class SetupStatementForUse{
    public:

        SetupStatementForUse(PreparedStatement &statement) : Statement(statement){

        }

        SetupStatementForUse(SetupStatementForUse&& other) : Statement(other.Statement){

            other.DontReset = true;
        }

        SetupStatementForUse(const SetupStatementForUse &other) = delete;
        SetupStatementForUse& operator =(const SetupStatementForUse &other) = delete;
        
        ~SetupStatementForUse(){

            Statement.Reset();
        }

        PreparedStatement& Statement;
        bool DontReset = false;
    };

    enum class STEP_RESULT {

        ROW,
        COMPLETED
    };

    //! \brief Creates a statement
    PreparedStatement(sqlite3* sqlite, const char* str, size_t length);

    //! \brief Creates a statement
    PreparedStatement(sqlite3* sqlite, const std::string &str);

    ~PreparedStatement(){

        sqlite3_finalize(Statement);
    }

    //! Call after doing steps to reset for next use
    void Reset(){

        CurrentBindIndex = 1;
        sqlite3_reset(Statement);
    }

    //! \brief Setups this statement for use in Step
    template<typename... TBindTypes>
        SetupStatementForUse Setup(const TBindTypes&... valuestobind)
    {
        
        Reset();
        BindAll(valuestobind...);
        return SetupStatementForUse(*this);
    }

    //! \brief Steps the statement forwards, automatically throws if fails
    //! \param isprepared A created object that makes sure this object is setup correctly
    STEP_RESULT Step(const SetupStatementForUse &isprepared){

        const auto result = sqlite3_step(Statement);

        if(result == SQLITE_DONE){

            // Finished //
            return STEP_RESULT::COMPLETED;
        }

        if(result == SQLITE_BUSY){

            LOG_WARNING("SQL statement: database is busy, retrying...");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return Step(isprepared);
        }

        if(result == SQLITE_ROW){

            // A row can be read //
            return STEP_RESULT::ROW;
        }

        // An error! //
        LOG_ERROR("An error occured in an sql statement, code: " + Convert::ToString(result));
        ThrowErrorFromDB(DB, result);
        
        // Execution never reaches this, as the above function will always throw
        return STEP_RESULT::COMPLETED;
    }

    //! \brief Steps until statement is completed, ignores any rows that might be returned
    void StepAll(const SetupStatementForUse &isprepared){

        while(Step(isprepared) != STEP_RESULT::COMPLETED){

        }
    }

    // Row functions
    // \note These should only be called when Step has returned a row
    
    auto GetColumnCount() const{

        return sqlite3_column_count(Statement);
    }

    //! This will return one of:
    //! SQLITE_INTEGER
    //! SQLITE_FLOAT 
    //! SQLITE_BLOB 
    //! SQLITE_NULL 
    //! SQLITE_TEXT
    auto GetColumnType(int column) const{

        return sqlite3_column_type(Statement, column);
    }

    bool IsColumnNull(int column) const{

        return GetColumnType(column) == SQLITE_NULL;
    }

    auto GetColumnName(int column){
        
        const char* name = sqlite3_column_name(Statement, column);
        return name ? std::string(name) : std::string("unknown");
    }

    const char* GetColumnNameDirect(int column){

        return sqlite3_column_name(Statement, column);
    }

    // Warning these functions will do inplace conversions and the types
    // will be inconsistent after these calls. You should first call GetColumnType
    // and then one of these get as functions based on the type
    
    auto GetColumnAsInt(int column){

        return sqlite3_column_int(Statement, column);
    }

    auto GetColumnAsBool(int column){

        int value = sqlite3_column_int(Statement, column);

        return value != 0 ? true : false;
    }

    auto GetColumnAsInt64(int column){

        return sqlite3_column_int64(Statement, column);
    }

    auto GetColumnAsString(int column){

        auto* str = sqlite3_column_text(Statement, column);
        return str ? std::string(reinterpret_cast<const char*>(str)) : std::string();
    }

    //! \brief Sets id to be the value from column
    //! \returns True if column is valid and not null
    bool GetObjectIDFromColumn(DBID &id, int column = 0){

        if(column >= GetColumnCount())
            return false;

        if(IsColumnNull(column) || GetColumnType(column) != SQLITE_INTEGER)
            return false;

        id = GetColumnAsInt64(column);
        return true;
    }

    // Binding functions for parameters
    template<typename TParam, typename = std::enable_if<false>>
        void SetBindWithType(int index, const TParam &value)
    {
        static_assert(sizeof(TParam) == -1, "SetBindWithType non-specialized version called");
    }


    template<typename TParamType>
        PreparedStatement& Bind(const TParamType &value)
    {
        SetBindWithType(CurrentBindIndex, value);
        ++CurrentBindIndex;
        return *this;
    }

    PreparedStatement& BindAll(){

        return *this;
    }

    template<typename TFirstParam, typename... TRestOfTypes>
        PreparedStatement& BindAll(const TFirstParam &value,
            const TRestOfTypes&... othervalues)
    {
        Bind(value);
        return BindAll(othervalues...);
    }

    //! \brief Throws if returncode isn't SQLITE_OK
    inline void CheckBindSuccess(int returncode, int index){

        if(returncode == SQLITE_OK)
            return;

        auto* desc = sqlite3_errstr(returncode);

        throw InvalidSQL("Binding argument at index " + Convert::ToString(index) + " failed",
            returncode, desc ? desc : "no description");        
    }

    sqlite3* DB;
    sqlite3_stmt* Statement;

    //! Contains the left over parts of the statement, empty if only one statement was passed
    std::string UncompiledPart;
    
    int CurrentBindIndex = 1;
    
};

template<>
    void PreparedStatement::SetBindWithType(int index, const int &value);

template<>
    void PreparedStatement::SetBindWithType(int index, const int64_t &value);

template<>
    void PreparedStatement::SetBindWithType(int index, const std::string &value);

template<>
    void PreparedStatement::SetBindWithType(int index, const std::nullptr_t &value);

template<>
    void PreparedStatement::SetBindWithType(int index, const bool &value);



inline void CheckRowID(PreparedStatement &statement, int index, const char* name){

    auto* columnName = statement.GetColumnNameDirect(index);
    LEVIATHAN_ASSERT(columnName, "Column name retrieval for verification failed");
    LEVIATHAN_ASSERT(strcmp(name, columnName) == 0,
        "SQL returned row columns are unexpected");
}


}
