// ------------------------------------ //
#include "Image.h"

#include "DualView.h"
#include "CacheManager.h"

#include <boost/filesystem.hpp>

using namespace DV;
// ------------------------------------ //

Image::Image(const std::string &file) : ResourcePath(file), ImportLocation(file)
{
    ResourceName = boost::filesystem::path(ResourcePath).filename().string();
    Extension = boost::filesystem::path(ResourcePath).extension().string();
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

