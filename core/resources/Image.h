#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <atomic>

namespace DV{

class LoadedImage;

class DualView;

//! \brief Main class for all image files that are handled by DualView
//!
//! This may or may not be in the database currently. The actual image
//! data will be loaded by CacheManager
//! \note Once the image has been loaded this will be made a duplicate of
//! another image that is in the database IF the hashes match
class Image{
    friend DualView;
public:

    //! \brief Creates a non-db version of an Image.
    //! \exception Leviathan::InvalidArgument if something is wrong with the filen
    Image(const std::string &file);
    
    //! \brief Returns the full sized image
    std::shared_ptr<LoadedImage> GetImage() const;
    
    //! \brief Returns the thumbnail image
    //! \note Will return null if hash hasn't been calculated yet
    std::shared_ptr<LoadedImage> GetThumbnail() const;


    //! \brief Returns a hash calculated from the file at ResourcePath
    //! \note This takes a while and should be called from a background thread
    //! \returns A base64 encoded sha256 of the entire file contents. With /'s replaced
    //! with _'s
    std::string CalculateFileHash() const;

protected:

    //! \brief Called after _DoHashCalculation if the calculated hash already
    //! existed
    //! \post This needs to act just like if the other instance was used
    void _MakeDuplicateOf(const Image &other);

    //! \brief Called by DualView from a worker thread to calculate the hash
    //! for this image
    void _DoHashCalculation();
    
private:

    //! True when Hash has been calculated
    std::atomic<bool> IsReadyToAdd = { false };

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
    
