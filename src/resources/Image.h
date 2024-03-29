#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "DatabaseResource.h"
#include "ResourceWithPreview.h"
#include "TimeHelpers.h"
#include "Exceptions.h"

namespace DV
{

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
class Image : public ResourceWithPreview,
              public DatabaseResource,
              public std::enable_shared_from_this<Image>
{
    friend DualView;
    friend Database;
    friend MaintenanceTools;

protected:
    //! \brief Creates a non-db version of an Image.
    //! \exception Leviathan::InvalidArgument if something is wrong with the file
    explicit Image(const std::string& file);

    //! \see Image::Create
    Image(const std::string& file, const std::string& name, const std::string& importoverride);

    //! \brief Subclass constructor for empty images, will assert if default functions
    //! get called
    Image();

    //! \brief Constructor for database loading
    Image(Database& db, DatabaseLockT& dblock, PreparedStatement& statement, int64_t id);

    //! \brief Init that must be called after a shared_ptr to this instance is created
    //!
    //! Called by Create functions
    virtual void Init();

public:
    ~Image() override;

    //! \brief Loads a database image
    //! \exception InvalidSQL if data is missing in the statement
    static inline std::shared_ptr<Image> Create(
        Database& db, DatabaseLockT& dblock, PreparedStatement& statement, int64_t id)
    {
        auto obj = std::shared_ptr<Image>(new Image(db, dblock, statement, id));
        obj->Init();
        return obj;
    }

    //! \brief Creates a non-db version of an Image.
    //! \exception Leviathan::InvalidArgument if something is wrong with the file
    static inline std::shared_ptr<Image> Create(const std::string& file)
    {
        auto obj = std::shared_ptr<Image>(new Image(file));
        obj->Init();
        return obj;
    }

    //! \brief Creates a non-db version of an Image.
    //! \exception Leviathan::InvalidArgument if something is wrong with the file
    //! \param importoverride Sets the import location property
    static inline std::shared_ptr<Image> Create(
        const std::string& file, const std::string& name, const std::string& importoverride)
    {
        auto obj = std::shared_ptr<Image>(new Image(file, name, importoverride));
        obj->Init();
        return obj;
    }

    //! \brief Returns the full sized image
    virtual std::shared_ptr<LoadedImage> GetImage();

    //! \brief Returns the thumbnail image
    //! \note Will return null if hash hasn't been calculated yet
    virtual std::shared_ptr<LoadedImage> GetThumbnail();

    //! \brief Returns the hash
    //! \exception Leviathan::InvalidState if hash hasn't been calculated yet
    std::string GetHash() const;

    //! \brief Returns a tag collection
    //!
    //! If this is in the database then the collection will automatically save to the database.
    //! If this isn't in the database the collection won't be saved unless this image
    //! is imported to the collection in DualView::AddToCollection
    [[nodiscard]] const std::shared_ptr<TagCollection>& GetTags()
    {
        return Tags;
    }

    //! \brief Returns true if this is ready to be added to the database
    [[nodiscard]] inline bool IsReady() const
    {
        return IsReadyToAdd;
    }

    //! \brief Returns true if there hasn't been an error with this image
    [[nodiscard]] inline auto GetIsValid() const
    {
        return IsValid;
    }

    //! \returns True if hash calculation has been attempted but it failed
    [[nodiscard]] inline bool IsHashInvalid() const
    {
        return HashCalculateAttempted && !IsHashValid;
    }

    //! \brief Returns a shared_ptr pointing to this instance
    [[nodiscard]] inline std::shared_ptr<Image> GetPtr()
    {
        return shared_from_this();
    }

    //! \brief Returns the name
    [[nodiscard]] inline const std::string& GetName() const
    {
        return ResourceName;
    }

    [[nodiscard]] inline const auto& GetResourcePath() const
    {
        return ResourcePath;
    }

    [[nodiscard]] inline const auto&  GetExtension() const
    {
        return Extension;
    }

    [[nodiscard]] inline auto GetWidth() const
    {
        return Width;
    }

    [[nodiscard]] inline auto GetHeight() const
    {
        return Height;
    }

    [[nodiscard]]  inline auto GetPixelCount() const
    {
        return Width * Height;
    }

    [[nodiscard]] inline auto GetIsPrivate() const
    {
        return IsPrivate;
    }

    [[nodiscard]] inline const auto& GetFromFile() const
    {
        return ImportLocation;
    }

    [[nodiscard]] std::string GetAddDateStr() const
    {
        return TimeHelpers::format8601(AddDate);
    }

    [[nodiscard]] std::string GetLastViewStr() const
    {
        return TimeHelpers::format8601(LastView);
    }

    //! \copydoc Image::Deleted
    [[nodiscard]] bool IsDeleted() const
    {
        return Deleted;
    }

    //! \copydoc Image::Merged
    [[nodiscard]] bool IsMerged() const
    {
        return Merged;
    }

    //! \brief Updates the resources location. Must be called after the file at ResourcePath
    //! is moved
    void SetResourcePath(const std::string& newpath);

    //! \brief Updates the signature. Marks this as changed. This will be saved later
    void SetSignature(const std::string& signature);

    //! \note Loads the signature from DB if not loaded already
    [[nodiscard]] const std::string& GetSignature();

    //! \brief Returns true if the signature is loaded
    [[nodiscard]] bool HasSignatureRetrieved() const
    {
        return SignatureRetrieved;
    }

    [[nodiscard]] std::string GetSignatureBase64();

    //! \brief Returns a hash calculated from the file at ResourcePath
    //! \note This takes a while and should be called from a background thread
    //! \returns A base64 encoded sha256 of the entire file contents. With /'s replaced
    //! with _'s
    [[nodiscard]] std::string CalculateFileHash() const;

    //! \brief Returns true if the images represent the same image
    //! \todo Fix this
    bool operator==(const Image& other) const;

    // Implementation of ResourceWithPreview
    std::shared_ptr<ListItem> CreateListItem(const std::shared_ptr<ItemSelectable>& selectable) override;
    bool IsSame(const ResourceWithPreview& other) override;
    bool UpdateWidgetWithValues(ListItem& control) override;

protected:
    // DatabaseResource implementation
    void _DoSave(Database& db) override;
    void _DoSave(Database& db, DatabaseLockT& dblock) override;
    void _OnAdopted() override;
    void _OnPurged() override;

    //! Called from Database
    void _UpdateDeletedStatus(bool deleted)
    {
        Deleted = deleted;

        GUARD_LOCK();
        NotifyAll(guard);
    }

    //! Called from Database
    void _UpdateMergedStatus(bool merged)
    {
        Merged = merged;

        GUARD_LOCK();
        NotifyAll(guard);
    }

    void ForceUnDeleteToFixMissingAction()
    {
        if (!Deleted)
            throw Leviathan::Exception("This needs to be in deleted state to call this fix missing action");

        Deleted = false;
    }

    //! \brief Once hash is calculated this is called if this is a duplicate of an
    //! existing image
    //! \warning Update events won't be shared between the two instances. It is still safe
    //! to import such an image but to edit the tags from multiple places at once
    //! will lead to the other not seeing the changes instantly, but is shouldn't actually
    //! break anything
    void BecomeDuplicateOf(const Image& other);

    //! \brief Queues DualView to calculate this hash
    void _QueueHashCalculation();

    //! \brief Called after _DoHashCalculation if the calculated hash already
    //! existed
    //! \post This needs to act just like if the other instance was used
    void _MakeDuplicateOf(const Image& other);

    //! \brief Called by DualView from a worker thread to calculate the hash
    //! for this image
    void _DoHashCalculation();

    //! \brief Called after _DoHashCalculation if this wasn't a duplicate
    void _OnFinishHash();

    //! \brief Fills a widget with this resource
    void _FillWidget(ImageListItem& widget);

protected:
    //! True when Hash has been calculated and duplicate check has completed
    std::atomic<bool> IsReadyToAdd = {false};

    //! Set to false if image is invalid format
    bool IsValid = true;

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
    std::string HashError;
    bool HashCalculateAttempted = false;

    int Height = 0;
    int Width = 0;

    std::shared_ptr<TagCollection> Tags;

    //! LibPuzzle signature
    //! This is stored in another table
    std::string Signature;
    bool SignatureRetrieved = false;

    //! If true deleted (or marked deleted) from the database and many things should skip this
    //! image
    bool Deleted = false;

    //! If true this has been merged into another Image (Deleted should usually be true when
    //! this is true)
    //! \note This is not stored in the DB so this is only available after merging until all
    //! references to this object are removed
    bool Merged = false;
};

} // namespace DV
