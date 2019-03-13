// ------------------------------------ //
#include "SQLHelpers.h"

#include "Common.h"

#include <sqlite3.h>

using namespace DV;
// ------------------------------------ //

void DV::ThrowErrorFromDB(sqlite3* sqlite, int code /*= 0*/, const std::string& extramessage)
{
    if(code == 0)
        code = sqlite3_errcode(sqlite);

    auto* msg = sqlite3_errmsg(sqlite);
    auto* desc = sqlite3_errstr(code);

    const auto* msgStr = msg ? msg : "no message";

    throw InvalidSQL(
        !extramessage.empty() ? (msgStr + std::string(", While: ") + extramessage) : msgStr,
        code, desc ? desc : "no description");
}

// ------------------------------------ //
// InvalidSQL
InvalidSQL::InvalidSQL(
    const std::string& message, int32_t code, const std::string& codedescription) noexcept :
    ErrorCode(code)
{
    FinalMessage = "[SQL EXCEPTION] ([" + Convert::ToString(ErrorCode) + "] " +
                   codedescription + "): " + message;
}
const char* InvalidSQL::what() const noexcept
{
    return FinalMessage.c_str();
}

void InvalidSQL::PrintToLog() const noexcept
{
    LOG_WRITE(FinalMessage);
}
