// ------------------------------------ //
#include "Collection.h"

#include "Common.h"

#include "Common/StringOperations.h"
#include <boost/filesystem.hpp>

using namespace DV;
// ------------------------------------ //
Collection::Collection(const std::string &name) : Name(name){

}

// ------------------------------------ //
std::string Collection::GetNameForFolder() const{

    LEVIATHAN_ASSERT(!Name.empty(), "GetNameForFolder called when Name is empty");

    std::string sanitized = Leviathan::StringOperations::ReplaceSingleCharacter<std::string>(
        Name, "\\/<>:\"|", ' ');

    // And then get rid of all characters under 0x1F
    for(auto iter = sanitized.begin(); iter != sanitized.end(); ++iter){

        if((*iter) <= 0x1F)
            (*iter) = ' ';
    }

    // Also may not be only dots //
    if(sanitized.find_first_not_of('.') == std::string::npos){

        sanitized = "padded_" + sanitized;
    }

    // May not end with a dot
    if(sanitized.back() == '.')
        sanitized += "d";

    // Don't start with a dot or an hyphen //
    if(sanitized.front() == '.' || sanitized.front() == '-')
        sanitized = "d" + sanitized;

    // Verify that it is a valid name //
    if(!boost::filesystem::windows_name(sanitized)){

        LOG_FATAL("Failed to sanitize file name: '" + sanitized + "' is not valid");
        return "";
    }

    return sanitized;
}

