// ------------------------------------ //
#include "Collection.h"

#include "Common.h"

#include "core/Database.h"
#include "core/PreparedStatement.h"

#include "Database.h"
#include "DualView.h"
#include "Database.h"

#include "Common/StringOperations.h"
#include <boost/filesystem.hpp>

using namespace DV;
// ------------------------------------ //
// Non-database testing version
Collection::Collection(const std::string &name) :
    DatabaseResource(true), Name(name),

    AddDate(date::make_zoned(date::current_zone(),
            std::chrono::time_point_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now()))),

    ModifyDate(date::make_zoned(date::current_zone(),
            std::chrono::time_point_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now()))),

    LastView(date::make_zoned(date::current_zone(),
            std::chrono::time_point_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now())))
{

}

Collection::Collection(Database &db, Lock &dblock, PreparedStatement &statement, int64_t id) :
    DatabaseResource(id, db),

    AddDate(TimeHelpers::parse8601(statement.GetColumnAsString(2))),
    ModifyDate(TimeHelpers::parse8601(statement.GetColumnAsString(3))),
    LastView(TimeHelpers::parse8601(statement.GetColumnAsString(4)))
{
    // Load properties //
    CheckRowID(statement, 1, "name");
    CheckRowID(statement, 2, "add_date");
    CheckRowID(statement, 3, "modify_date");
    CheckRowID(statement, 4, "last_view");
    CheckRowID(statement, 5, "is_private");
    CheckRowID(statement, 6, "preview_image");

    Name = statement.GetColumnAsString(1);
    IsPrivate = statement.GetColumnAsBool(5);
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
// ------------------------------------ //
bool Collection::AddTags(const TagCollection &tags){

    if(!Tags)
        return false;

    Tags->AddTags(tags);
    return true;
}

int64_t Collection::GetLastShowOrder(){

    if(!IsInDatabase())
        return 0;
    
    return InDatabase->SelectCollectionLargestShowOrder(*this);
}
// ------------------------------------ //
bool Collection::AddImage(std::shared_ptr<Image> image){

    if(!image || !IsInDatabase())
        return false;
    
    return InDatabase->InsertImageToCollection(*this, *image,
        GetLastShowOrder() + 1);
}

bool Collection::AddImage(std::shared_ptr<Image> image, int64_t order){

    if(!image || !IsInDatabase())
        return false;
    
    return InDatabase->InsertImageToCollection(*this, *image, order);
}

bool Collection::RemoveImage(std::shared_ptr<Image> image){

    if(!image || !IsInDatabase())
        return false;
    
    return InDatabase->DeleteImageFromCollection(*this, *image);
}

int64_t Collection::GetImageCount(){

    if(!IsInDatabase())
        return 0;

    return InDatabase->SelectCollectionImageCount(*this);
}

int64_t Collection::GetImageShowOrder(std::shared_ptr<Image> image){

    if(!image || !IsInDatabase())
        return -1;

    return InDatabase->SelectImageShowOrderInCollection(*this, *image);
}

// ------------------------------------ //
void Collection::_DoSave(Database &db){

    db.UpdateCollection(*this);
}
