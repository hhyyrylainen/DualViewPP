#pragma once

#include "DatabaseResource.h"
#include "ResourceWithPreview.h"
#include "Tags.h"

#include "components/ImageListScroll.h"

#include "TimeHelpers.h"

#include "Common/Types.h"

#include <memory>
#include <string>

namespace DV {

class Image;
class DatabaseTagCollection;
class CollectionListItem;

class Database;
class PreparedStatement;

class Collection : public DatabaseResource,
                   public ResourceWithPreview,
                   public ImageListScroll,
                   public std::enable_shared_from_this<Collection> {
public:
    //! \brief Creates a collection for database testing
    //! \protected
    Collection(const std::string& name);

public:
    //! \brief Database load function
    Collection(Database& db, DatabaseLockT& dblock, PreparedStatement& statement, int64_t id);

    ~Collection();

    //! \brief Return Name with illegal characters replaced with spaces
    //! \todo Make work with utf8 strings better
    std::string GetNameForFolder() const;

    //! \brief Returns or loads a tag collection for this Collection
    std::shared_ptr<TagCollection> GetTags();

    //! \brief Adds tags to this collection
    //! \note Only works if this is in the database
    //! \return True if successfully added
    bool AddTags(const TagCollection& tags, DatabaseLockT& dblock);


    //! \brief Adds an image to this Collection
    //!
    //! The images show_order will be set to be the last image in the collection
    bool AddImage(std::shared_ptr<Image> image);

    //! \see AddImage
    bool AddImage(std::shared_ptr<Image> image, DatabaseLockT& dblock);

    //! \brief Adds an image to this Collection
    //!
    //! The images show_order will be set to be the last image in the collection
    bool AddImage(std::shared_ptr<Image> image, int64_t order);

    //! \see AddImage
    bool AddImage(std::shared_ptr<Image> image, int64_t order, DatabaseLockT& dblock);

    //! \brief Removes an image from this collection
    //! \returns True if the image was removed, false if it wasn't in this collection
    bool RemoveImage(std::shared_ptr<Image> image, DatabaseLockT& dblock);

    //! \brief Removes images from this collection
    //! \returns True if the image was removed, false if it wasn't in this collection or
    //! something else failed
    bool RemoveImage(const std::vector<std::shared_ptr<Image>>& images);

    //! \brief Gets the largest show_order used in the collection
    int64_t GetLastShowOrder() const;

    //! \see GetLastShowOrder
    int64_t GetLastShowOrder(DatabaseLockT& dblock) const;

    //! \brief Returns the image count
    int64_t GetImageCount() const;

    //! \see GetImageCount
    int64_t GetImageCount(DatabaseLockT& dblock) const;

    //! \brief Returns image's show_order in this collection. Or -1
    int64_t GetImageShowOrder(std::shared_ptr<Image> image) const;

    //! \see GetImageShowOrder
    int64_t GetImageShowOrder(std::shared_ptr<Image> image, DatabaseLockT& dblock) const;

    //! \brief Returns the preview icon for this Collection
    //!
    //! It is either the first image or a specifically set image
    std::shared_ptr<Image> GetPreviewIcon() const;

    //! \brief Returns all the images in the collection
    std::vector<std::shared_ptr<Image>> GetImages() const;

    inline auto GetIsPrivate() const
    {

        return IsPrivate;
    }

    inline auto GetName() const
    {

        return Name;
    }

    //! \brief Returns true if both Collections represent the same database collection
    bool operator==(const Collection& other) const;

    // Implementation of ResourceWithPreview
    std::shared_ptr<ListItem> CreateListItem(
        const std::shared_ptr<ItemSelectable>& selectable) override;
    bool IsSame(const ResourceWithPreview& other) override;
    bool UpdateWidgetWithValues(ListItem& control) override;

    // ImageListScroll implementation //
    std::shared_ptr<Image> GetNextImage(
        std::shared_ptr<Image> current, bool wrap = true) override;

    std::shared_ptr<Image> GetPreviousImage(
        std::shared_ptr<Image> current, bool wrap = true) override;

    bool HasCount() const override;
    size_t GetCount() const override;
    bool SupportsRandomAccess() const override;

    std::shared_ptr<Image> GetImageAt(size_t index) const override;
    size_t GetImageIndex(Image& image) const override;

    std::string GetDescriptionStr() const override;

protected:
    // DatabaseResource implementation
    void _DoSave(Database& db) override;

    //! \brief Fills a widget with this resource
    void _FillWidget(CollectionListItem& widget);

protected:
    std::string Name;

    date::zoned_time<std::chrono::milliseconds> AddDate;
    date::zoned_time<std::chrono::milliseconds> ModifyDate;
    date::zoned_time<std::chrono::milliseconds> LastView;

    bool IsPrivate = false;

    std::shared_ptr<TagCollection> Tags;
};

} // namespace DV
