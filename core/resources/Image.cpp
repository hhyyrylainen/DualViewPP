// ------------------------------------ //
#include "Image.h"

#include "DualView.h"
#include "CacheManager.h"
#include "Exceptions.h"
#include "FileSystem.h"
#include "Common/StringOperations.h"

// TODO: remove when DEBUG_BREAK is gone
#include "Common.h"

#include <boost/filesystem.hpp>

using namespace DV;
// ------------------------------------ //

Image::Image(const std::string &file) : ResourcePath(file), ImportLocation(file)
{
    if(!boost::filesystem::exists(file)){

        throw Leviathan::InvalidArgument("Image: file doesn't exist");
    }
    
    ResourceName = boost::filesystem::path(ResourcePath).filename().string();
    Extension = boost::filesystem::path(ResourcePath).extension().string();

    // Register hash calculation //
    
}
// ------------------------------------ //
std::shared_ptr<LoadedImage> Image::GetImage() const{

    return DualView::Get().GetCacheManager().LoadFullImage(ResourcePath);
}
    
std::shared_ptr<LoadedImage> Image::GetThumbnail() const{

    if(!IsReadyToAdd)
        return nullptr;

    return DualView::Get().GetCacheManager().LoadThumbImage(ResourcePath, Hash);
}

// ------------------------------------ //
std::string Image::CalculateFileHash() const{

    // Load file bytes //
    std::string contents;
    if(!Leviathan::FileSystem::ReadFileEntirely(ResourcePath, contents)){

        LEVIATHAN_ASSERT(0, "Failed to read file for hash calculation");
    }

    DEBUG_BREAK;

    // Calculate sha256 hash //
    std::string hash = contents;


    // Encode it //
    hash = Leviathan::StringOperations::Replace<std::string>(hash, "/", "_");

    return contents;
}

void Image::_DoHashCalculation(){

    Hash = CalculateFileHash();


    IsReadyToAdd = true;
}

