// ------------------------------------ //
#include "NetGallery.h"

#include "core/Database.h"
#include "core/resources/InternetImage.h"
#include "core/resources/Tags.h"

#include "Exceptions.h"

using namespace DV;
// ------------------------------------ //
NetGallery::NetGallery(const std::string &url) :
    DatabaseResource(true), GalleryUrl(url)
{

}

NetGallery::NetGallery(Database &db, Lock &dblock, PreparedStatement &statement, int64_t id) :
    DatabaseResource(id, db)
{
    // Load properties //
    CheckRowID(statement, 1, "gallery_url");
    CheckRowID(statement, 2, "target_path");
    CheckRowID(statement, 3, "gallery_name");
    CheckRowID(statement, 4, "currently_scanned");
    CheckRowID(statement, 5, "is_downloaded");
    CheckRowID(statement, 6, "tags_string");

    GalleryUrl = statement.GetColumnAsString(1);

    TargetPath = statement.GetColumnAsString(2);
    
    TargetGalleryName = statement.GetColumnAsString(3);

    CurrentlyScanned = statement.GetColumnAsString(4);

    IsDownloaded = statement.GetColumnAsBool(5);

    TagsString = statement.GetColumnAsString(6);
}
    
NetGallery::~NetGallery(){

}
// ------------------------------------ //
void NetGallery::AddFilesToDownload(const std::vector<std::shared_ptr<InternetImage>> &images){

    if(!IsInDatabase())
        throw Leviathan::InvalidState("NetGallery not in database");

    for(const auto& image : images){

        std::string tags = "";

        if(image->GetTags()->HasTags()){

            tags = image->GetTags()->TagsAsString(";");
        }

        NetFile file(image->GetURL(), image->GetReferrer(), image->GetName(), tags);
        InDatabase->InsertNetFile(file, *this);
    }
}
// ------------------------------------ //
void NetGallery::_DoSave(Database &db){

    db.UpdateNetGallery(*this);
}

// ------------------------------------ //
// NetFile
NetFile::NetFile(const std::string &url, const std::string &referrer, const std::string &name,
    const std::string &tagstr /*= ""*/) :
    DatabaseResource(true), FileURL(url), PageReferrer(referrer), PreferredName(name),
    TagsString(tagstr)
{

}

NetFile::NetFile(Database &db, Lock &dblock, PreparedStatement &statement, int64_t id) :
    DatabaseResource(id, db)
{
    // Load properties //
    CheckRowID(statement, 1, "file_url");
    CheckRowID(statement, 2, "page_referrer");
    CheckRowID(statement, 3, "preferred_name");
    CheckRowID(statement, 4, "tags_string");

    FileURL = statement.GetColumnAsString(1);
    PageReferrer = statement.GetColumnAsString(2);
    PreferredName = statement.GetColumnAsString(3);
    TagsString = statement.GetColumnAsString(4);
}
// ------------------------------------ //
void NetFile::_DoSave(Database &db){

    db.UpdateNetFile(*this);
}
