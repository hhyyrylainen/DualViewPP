#pragma once

#include "ResourceWithPreview.h"
#include "DatabaseResource.h"

#include "core/TimeHelpers.h"

#include <string>
#include <memory>
#include <atomic>

namespace DV{

class LoadedImage;
class ImageListItem;
class DualView;
class TagCollection;

class PreparedStatement;

//! \brief Main class for all image files that are handled by DualView
//!
//! This may or may not be in the database currently. The actual image
//! data will be loaded by CacheManager
//! \note Once the image has been loaded this will be made a duplicate of
//! another image that is in the database IF the hashes match
class Image : public ResourceWithPreview, public DatabaseResource,
                public std::enable_shared_from_this<Image>
{
    friend DualView;

protected:

    //! \brief Creates a non-db version of an Image.
    //! \exception Leviathan::InvalidArgument if something is wrong with the file
    Image(const std::string &file);

    //! \brief Constructor for database loading
    Image(Database &db, Lock &dblock, PreparedStatement &statement, int64_t id);

    //! \brief Init that must be called after a shared_ptr to this instance is created
    //!
    //! Called by Create functions
    void Init();
    
public:

    //! \brief Loads a database image
    //! \exception InvalidSQL if data is missing in the statement
    inline static std::shared_ptr<Image> Create(Database &db, Lock &dblock,
        PreparedStatement &statement, int64_t id)
    {
        auto obj = std::shared_ptr<Image>(new Image(db, dblock, statement, id));
        obj->Init();
        return obj;
    }

    //! \brief Creates a non-db version of an Image.
    //! \exception Leviathan::InvalidArgument if something is wrong with the file
    //! \todo If the file path is in the database load that instead
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

    //! \brief Returns a tag collection
    //!
    //! If this is in the databse then the collection will automatically save to the database.
    //! If this isn't in the database the collection won't be saved unless this image
    //! is imported to the collection in DualView::AddToCollection
    std::shared_ptr<TagCollection> GetTags(){
        
        return Tags;
    }

    //! \brief Returns true if this is ready to be added to the database
    inline bool IsReady() const{

        return IsReadyToAdd;
    }

    //! \brief Returns a shared_ptr pointing to this instance
    inline std::shared_ptr<Image> GetPtr(){
        
        return shared_from_this();
    }

    //! \brief Returns the name
    inline std::string GetName() const{

        return ResourceName;
    }

    inline auto GetResourcePath() const{

        return ResourcePath;
    }

    inline auto GetExtension() const{

        return Extension;
    }

    inline auto GetWidth() const{

        return Width;
    }

    inline auto GetHeight() const{

        return Height;
    }

    inline auto GetIsPrivate() const{

        return IsPrivate;
    }

    inline auto GetFromFile() const{

        return ImportLocation;
    }

    std::string GetAddDateStr() const{

        return TimeHelpers::format8601(AddDate);
    }

    std::string GetLastViewStr() const{

        return TimeHelpers::format8601(LastView);
    }

    //! \brief Updates the resources location. Must be called after the file at ResourcePath
    //! is moved
    void SetResourcePath(const std::string &newpath);


    //! \brief Returns a hash calculated from the file at ResourcePath
    //! \note This takes a while and should be called from a background thread
    //! \returns A base64 encoded sha256 of the entire file contents. With /'s replaced
    //! with _'s
    std::string CalculateFileHash() const;

    //! \brief Returns true if the images represent the same image
    //! \todo Fix this
    bool operator ==(const Image& other);


    // Implementation of ResourceWithPreview
    std::shared_ptr<ListItem> CreateListItem(const ItemSelectable &selectable) override;
    bool IsSame(const ResourceWithPreview &other) override;
    bool UpdateWidgetWithValues(ListItem &control) override;

protected:

    // DatabaseResource implementation
    void _DoSave(Database &db) override;

    //! \brief Once hash is calculated this is called if this is a duplicate of an
    //! existing image
    //! \warning Update events won't be shared between the two instances. It is still safe
    //! to import such an image but to edit the tags from multiple places at once
    //! will lead to the other not seeing the changes instantly, but is shouldn't actually
    //! break anything
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

    //! \brief Fills a widget with this resource
    void _FillWidget(ImageListItem &widget);
    
private:

    //! True when Hash has been calculated and duplicate check has completed
    std::atomic<bool> IsReadyToAdd = { false };

    std::string ResourcePath;
    std::string ResourceName;

    std::string Extension;

    bool IsPrivate = false;
    
    date::zoned_time<std::chrono::milliseconds> AddDate;

    date::zoned_time<std::chrono::milliseconds> LastView;

    std::string ImportLocation;

    //! True if Hash has been set to a valid value
    bool IsHashValid = false;
    std::string Hash;

    int Height = 0;
    int Width = 0;

    std::shared_ptr<TagCollection> Tags;
};

}
    
