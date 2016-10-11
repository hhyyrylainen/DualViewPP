#pragma once

#include <exception>
#include <cinttypes>
#include <string>

//! \file Common classes for Database.h and PreparedStatement.h

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

//! \brief Throws an InvalidSQL exceptions from sqlite current error
void ThrowErrorFromDB(sqlite3* sqlite, int code = 0,
    const std::string &extramessage = "");

}
