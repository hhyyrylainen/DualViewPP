#pragma once

#include <string>
#include <memory>
#include <chrono>

namespace DV{

class LoadedImage;

//! \brief Main class for all image files that are handled by DualView
//!
//! This may or may not be in the database currently. The actual image
//! data will be loaded by CacheManager
//! \note Once the image has been loaded this will be made a duplicate of
//! another image that is in the database IF the hashes match
class Image{
public:

    //! \brief Creates a non-db version of an Image.
    //! \exception Leviathan::InvalidArgument if something is wrong with the filen
    Image(const std::string &file);
    
    //! \brief Returns the full sized image
    std::shared_ptr<LoadedImage> GetImage() const;
    
    //! \brief Returns the thumbnail image
    //! \note Will return null if hash hasn't been calculated yet
    std::shared_ptr<LoadedImage> GetThumbnail() const;
    
private:

    //! True when Hash has been calculated
    bool IsReadyToAdd = false;

    std::string ResourcePath;
    std::string ResourceName;

    std::string Extension;

    bool IsPrivate = false;
    std::chrono::system_clock::time_point AddDate = std::chrono::system_clock::now();

    std::chrono::system_clock::time_point LastView = std::chrono::system_clock::now();

    std::string ImportLocation;

    std::string Hash;


    int Height = 0;
    int Width = 0;
};

}
    
