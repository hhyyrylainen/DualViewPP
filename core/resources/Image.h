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
class Image : public std::enable_shared_from_this<Image> {
    friend DualView;

protected:

    //! \brief Creates a non-db version of an Image.
    //! \exception Leviathan::InvalidArgument if something is wrong with the file
    Image(const std::string &file);

    //! \brief Init that must be called after a shared_ptr to this instance is created
    //!
    //! Called by Create functions
    void Init();
    
public:

    //! \brief Creates a non-db version of an Image.
    //! \exception Leviathan::InvalidArgument if something is wrong with the file
    inline static std::shared_ptr<Image> Create(const std::string &file){

        auto obj = std::shared_ptr<Image>(new Image(file));
        obj->Init();
        return obj;
    }
    
    //! \brief Returns the full sized image
    std::shared_ptr<LoadedImage> GetImage() const;
    
    //! \brief Returns the thumbnail image
    //! \note Will return null if hash hasn't been calculated yet
    std::shared_ptr<LoadedImage> GetThumbnail() const;

    //! \brief Returns the hash
    //! \exception Leviathan::InvalidState if hash hasn't been calculated yet
    std::string GetHash() const;

    //! \brief Returns true if this is ready to be added to the database
    inline bool IsReady() const{

        return IsReadyToAdd;
    }

    //! \brief Returns a shared_ptr pointing to this instance
    inline std::shared_ptr<Image> GetPtr(){
        
        return shared_from_this();
    }

    //! \brief Returns a hash calculated from the file at ResourcePath
    //! \note This takes a while and should be called from a background thread
    //! \returns A base64 encoded sha256 of the entire file contents. With /'s replaced
    //! with _'s
    std::string CalculateFileHash() const;

protected:

    //! \brief Once hash is calculated this is called if this is a duplicate of an
    //! existing image
    void BecomeDuplicateOf(const Image &other);

    //! \brief Queues DualView to calculate this hash
    void _QueueHashCalculation();

    //! \brief Called after _DoHashCalculation if the calculated hash already
    //! existed
    //! \post This needs to act just like if the other instance was used
    void _MakeDuplicateOf(const Image &other);

    //! \brief Called by DualView from a worker thread to calculate the hash
    //! for this image
    void _DoHashCalculation();

    //! \brief Called after _DoHashCalculation if this wasn't a duplicate
    void _OnFinishHash();
    
private:

    //! True when Hash has been calculated and duplicate check has completed
    std::atomic<bool> IsReadyToAdd = { false };

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
};

}
    
