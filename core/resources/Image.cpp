// ------------------------------------ //
#include "Image.h"

#include "DualView.h"
#include "CacheManager.h"
#include "Exceptions.h"

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

    DEBUG_BREAK;
    return "";
}

void Image::_DoHashCalculation(){

    Hash = CalculateFileHash();


    IsReadyToAdd = true;
}

