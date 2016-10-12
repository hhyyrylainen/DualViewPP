// ------------------------------------ //
#include "Image.h"

#include "core/components/ImageListItem.h"

#include "Tags.h"

#include "DualView.h"

#include "core/Database.h"
#include "core/PreparedStatement.h"

#include "CacheManager.h"
#include "Exceptions.h"
#include "FileSystem.h"
#include "Common/StringOperations.h"
#include "Common.h"

#include <boost/filesystem.hpp>
#include <cryptopp/sha.h>

#include "third_party/base64.h"

using namespace DV;
// ------------------------------------ //

Image::Image(const std::string &file) :
    DatabaseResource(true), ResourcePath(file), ImportLocation(file)
{
    if(!boost::filesystem::exists(file)){

        throw Leviathan::InvalidArgument("Image: file doesn't exist");
    }
    
    ResourceName = boost::filesystem::path(ResourcePath).filename().string();
    Extension = boost::filesystem::path(ResourcePath).extension().string();

    Tags = std::make_shared<TagCollection>();

}

Image::Image(Database &db, Lock &dblock, PreparedStatement &statement, int64_t id) :
    DatabaseResource(id, db)
{
    IsReadyToAdd = true;
    IsHashValid = true;
    
    // Load properties //
    CheckRowID(statement, 1, "relative_path");
    CheckRowID(statement, 2, "width");
    CheckRowID(statement, 3, "height");
    CheckRowID(statement, 4, "name");
    CheckRowID(statement, 5, "extension");
    CheckRowID(statement, 6, "add_date");
    CheckRowID(statement, 6, "last_view");
    CheckRowID(statement, 6, "is_private");
    CheckRowID(statement, 6, "from_file");
    CheckRowID(statement, 6, "file_hash");

    std::string ResourcePath;
    std::string ResourceName;

    std::string Extension;

    bool IsPrivate = false;
    std::chrono::system_clock::time_point AddDate = std::chrono::system_clock::now();

    std::chrono::system_clock::time_point LastView = std::chrono::system_clock::now();

    std::string ImportLocation;

    //! True if Hash has been set to a valid value
    bool IsHashValid = false;
    std::string Hash;


    int Height = 0;
    int Width = 0;
}

void Image::Init(){

    if(!IsHashValid){
        // Register hash calculation //
        _QueueHashCalculation();
    }
}
// ------------------------------------ //
std::shared_ptr<LoadedImage> Image::GetImage() const{

    return DualView::Get().GetCacheManager().LoadFullImage(ResourcePath);
}
    
std::shared_ptr<LoadedImage> Image::GetThumbnail() const{

    if(!IsHashValid)
        return nullptr;

    return DualView::Get().GetCacheManager().LoadThumbImage(ResourcePath, Hash);
}

std::string Image::GetHash() const{

    if(!IsHashValid)
        throw Leviathan::InvalidState("Hash hasn't been calculated");

    return Hash;
}
// ------------------------------------ //
void Image::SetResourcePath(const std::string &newpath){

    if(!boost::filesystem::exists(newpath)){

        throw Leviathan::InvalidArgument("Image: update path: file doesn't exist");
    }

    ResourcePath = newpath;
    Extension = boost::filesystem::path(ResourcePath).extension().string();
}
// ------------------------------------ //
void Image::_DoSave(Database &db){

    db.UpdateImage(*this);
}
// ------------------------------------ //
std::string Image::CalculateFileHash() const{

    // Load file bytes //
    std::string contents;
    if(!Leviathan::FileSystem::ReadFileEntirely(ResourcePath, contents)){

        LEVIATHAN_ASSERT(0, "Failed to read file for hash calculation");
    }

    // Calculate sha256 hash //
    byte digest[CryptoPP::SHA256::DIGESTSIZE];

    CryptoPP::SHA256().CalculateDigest(digest, reinterpret_cast<const byte*>(
            contents.data()), contents.length());

    static_assert(sizeof(digest) == CryptoPP::SHA256::DIGESTSIZE, "sizeof funkyness");

    // Encode it //
    std::string hash = base64_encode(digest, sizeof(digest));

    // Make it path safe //
    hash = Leviathan::StringOperations::ReplaceSingleCharacter<std::string>(hash, "/", '_');

    return hash;
}

void Image::_DoHashCalculation(){

    Hash = CalculateFileHash();
    IsHashValid = true;

    LEVIATHAN_ASSERT(!Hash.empty(), "Image created an empty hash");

    // IsReadyToAdd will be set by DualView once it is confirmed that
    // this isn't a duplicate
}

void Image::_OnFinishHash(){

    IsReadyToAdd = true;
}
// ------------------------------------ //
void Image::_QueueHashCalculation(){

    DualView::Get().QueueImageHashCalculate(shared_from_this());
}
// ------------------------------------ //
void Image::BecomeDuplicateOf(const Image &other){

    DEBUG_BREAK;
}

bool Image::operator ==(const Image& other){

    if(static_cast<DatabaseResource&>(*this) ==
        static_cast<const DatabaseResource&>(other))
    {
        return true;
    }
    
    return ResourcePath == other.ResourcePath;
}
// ------------------------------------ //
std::shared_ptr<ListItem> Image::CreateListItem(){

    auto widget = std::make_shared<ImageListItem>();
    
    _FillWidget(*widget);

    return widget;
}

bool Image::IsSame(const ResourceWithPreview &other){

    auto* asThis = dynamic_cast<const Image*>(&other);

    if(!asThis)
        return false;

    // Check is the image same //
    return *this == *asThis;
}

bool Image::UpdateWidgetWithValues(ListItem &control){

    auto* asOurType = dynamic_cast<ImageListItem*>(&control);

    if(!asOurType)
        return false;

    // Update the properties //
    _FillWidget(*asOurType);
    return true;
}

void Image::_FillWidget(ImageListItem &widget){

    widget.SetImage(shared_from_this());
}

