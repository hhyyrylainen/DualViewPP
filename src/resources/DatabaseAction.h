#pragma once

#include "Common.h"
#include "DatabaseResource.h"
#include "ReversibleAction.h"

namespace Json {
class Value;
}

namespace DV {

class Database;
class PreparedStatement;
class Image;
class ResourceWithPreview;
class BaseWindow;

//! \brief Used to create the correct class from the action_history table
enum class DATABASE_ACTION_TYPE : int {
    ImageDelete = 1,
    ImageMerge,
    ImageRemovedFromCollection,
    CollectionReorder,
    NetGalleryDelete,
    Invalid /* must be always last value */
};

//! \brief All reversible database operations result in this that can then be used to reverse
//! it
//!
//! The action data is stored as JSON in the database and this class doesn't really understand
//! what this contains. The Database is given the JSON data for all processing
class DatabaseAction : public ReversibleAction,
                       public DatabaseResource,
                       public std::enable_shared_from_this<DatabaseAction> {
    friend Database;

protected:
    DatabaseAction();
    DatabaseAction(DBID id, Database& from);

public:
    virtual ~DatabaseAction();

    static std::shared_ptr<DatabaseAction> Create(
        Database& db, DatabaseLockT& dblock, PreparedStatement& statement, DBID id);

    virtual DATABASE_ACTION_TYPE GetType() const = 0;

    virtual std::string SerializeData() const;

    //! \note This may retrieve data from the database to fill the string
    virtual std::string GenerateDescription() const = 0;

    //! \note This does database loading
    virtual std::vector<std::shared_ptr<ResourceWithPreview>> LoadPreviewItems(
        int max = 10) const;

    virtual bool SupportsEditing() const
    {
        return false;
    }

    //! \brief Opens a window for editing this action
    //!
    //! Should only be called if SupportsEditing()
    virtual void OpenEditingWindow();

    bool IsDeleted() const
    {
        return Deleted;
    }

protected:
    // Overrides from ReversibleAction. These in turn call _Redo and _Undo in order to catch
    // any DB errors thrown from them
    bool DoRedo() override;
    bool DoUndo() override;

    //! Called when removed from the database. This will permanently delete any resources still
    //! held by this action. For example Image is first marked deleted and once the action is
    //! popped off the history then the Image is permanently deleted
    //! \note This needs to be called from derived classes to set Deleted
    virtual void _OnPurged() override;

    virtual void _SerializeCustomData(Json::Value& value) const = 0;

    void _ReportPerformedStatus(bool performed);

private:
    virtual void _Redo() = 0;
    virtual void _Undo() = 0;

    void _DoSave(Database& db) override;
    void _DoSave(Database& db, DatabaseLockT& dblock) override;

private:
    bool Deleted = false;
};

//! \brief Image was deleted (marked deleted)
class ImageDeleteAction final : public DatabaseAction {
    friend Database;
    friend std::shared_ptr<DatabaseAction> DatabaseAction::Create(
        Database& db, DatabaseLockT& dblock, PreparedStatement& statement, DBID id);

protected:
    ImageDeleteAction(DBID id, Database& from, bool performed, const std::string& customdata);

public:
    //! This is public just to make std::make_shared work
    //! \protected
    ImageDeleteAction(const std::vector<DBID>& images);
    ~ImageDeleteAction();

    DATABASE_ACTION_TYPE GetType() const override
    {
        return DATABASE_ACTION_TYPE::ImageDelete;
    }

    const auto& GetImagesToDelete() const
    {
        return ImagesToDelete;
    }

    std::string GenerateDescription() const override;
    std::vector<std::shared_ptr<ResourceWithPreview>> LoadPreviewItems(
        int max = 10) const override;

protected:
    void _OnPurged() override;

private:
    void _Redo() override;
    void _Undo() override;
    void _SerializeCustomData(Json::Value& value) const override;

private:
    std::vector<DBID> ImagesToDelete;
};


//! \brief Image(s) were merged
class ImageMergeAction final : public DatabaseAction {
    friend Database;
    friend std::shared_ptr<DatabaseAction> DatabaseAction::Create(
        Database& db, DatabaseLockT& dblock, PreparedStatement& statement, DBID id);

protected:
    ImageMergeAction(DBID id, Database& from, bool performed, const std::string& customdata);

public:
    //! This is public just to make std::make_shared work
    //! \protected
    ImageMergeAction(DBID mergetarget, const std::vector<DBID>& images);
    ~ImageMergeAction();

    DATABASE_ACTION_TYPE GetType() const override
    {
        return DATABASE_ACTION_TYPE::ImageMerge;
    }

    //! \brief Determines if this action contains the specified data
    //!
    //! This is used to not have to recreate actions in some cases where a previous one can be
    //! redone
    bool IsSame(const Image& target, const std::vector<std::shared_ptr<Image>>& images) const;

    //! \brief Updates the properties of this action. This should not be in performed state
    //! when the change is made
    void UpdateProperties(DBID target, const std::vector<DBID>& imagestomerge);

    const auto GetTarget() const
    {
        return Target;
    }

    const auto& GetImagesToMerge() const
    {
        return ImagesToMerge;
    }

    auto GetPropertiesToAddToTarget() const
    {
        return std::tie(AddTargetToCollections, AddTagsToTarget);
    }

    const auto& GetAddTargetToCollections() const
    {
        return AddTargetToCollections;
    }

    const auto& GetAddTagsToTarget() const
    {
        return AddTagsToTarget;
    }

    std::string GenerateDescription() const override;
    //! \brief Returns the merge target as the first image and then the merged images
    std::vector<std::shared_ptr<ResourceWithPreview>> LoadPreviewItems(
        int max = 10) const override;

    bool SupportsEditing() const override
    {
        return true;
    }

    void OpenEditingWindow() override;

protected:
    void _OnPurged() override;

    void SetPropertiesToAddToTarget(
        const std::vector<std::tuple<DBID, int>>& addtargettocollections,
        const std::vector<std::string>& addtagstotarget)
    {
        AddTargetToCollections = addtargettocollections;
        AddTagsToTarget = addtagstotarget;
        OnMarkDirty();
    }

private:
    void _Redo() override;
    void _Undo() override;
    void _SerializeCustomData(Json::Value& value) const override;

private:
    DBID Target;
    std::vector<DBID> ImagesToMerge;

    std::vector<std::tuple<DBID, int>> AddTargetToCollections;
    std::vector<std::string> AddTagsToTarget;
};



//! \brief Image(s) were removed from a collection
class ImageDeleteFromCollectionAction final : public DatabaseAction {
    friend Database;
    friend std::shared_ptr<DatabaseAction> DatabaseAction::Create(
        Database& db, DatabaseLockT& dblock, PreparedStatement& statement, DBID id);

protected:
    ImageDeleteFromCollectionAction(
        DBID id, Database& from, bool performed, const std::string& customdata);

public:
    //! This is public just to make std::make_shared work
    //! \protected
    ImageDeleteFromCollectionAction(
        DBID collection, const std::vector<std::tuple<DBID, int64_t>>& images);
    ~ImageDeleteFromCollectionAction();

    DATABASE_ACTION_TYPE GetType() const override
    {
        return DATABASE_ACTION_TYPE::ImageRemovedFromCollection;
    }

    const auto& GetImagesToDelete() const
    {
        return ImagesToDelete;
    }

    const auto& GetAddedToUncategorized() const
    {
        return ImagesAddedToUncategorized;
    }

    auto GetDeletedFromCollection() const
    {
        return DeletedFromCollection;
    }


    std::string GenerateDescription() const override;
    std::vector<std::shared_ptr<ResourceWithPreview>> LoadPreviewItems(
        int max = 10) const override;

protected:
    void _OnPurged() override {}

    void SetAddedToUncategorized(const std::vector<DBID>& addedtouncategorized)
    {
        ImagesAddedToUncategorized = addedtouncategorized;
        OnMarkDirty();
    }

private:
    void _Redo() override;
    void _Undo() override;
    void _SerializeCustomData(Json::Value& value) const override;

private:
    DBID DeletedFromCollection;
    //! The removed images along with their original show order positions
    std::vector<std::tuple<DBID, int64_t>> ImagesToDelete;

    std::vector<DBID> ImagesAddedToUncategorized;
};


//! \brief Images in a Collection were reordered
class CollectionReorderAction final : public DatabaseAction {
    friend Database;
    friend std::shared_ptr<DatabaseAction> DatabaseAction::Create(
        Database& db, DatabaseLockT& dblock, PreparedStatement& statement, DBID id);

protected:
    CollectionReorderAction(
        DBID id, Database& from, bool performed, const std::string& customdata);

public:
    //! This is public just to make std::make_shared work
    //! \protected
    CollectionReorderAction(DBID collection, const std::vector<DBID>& images);
    ~CollectionReorderAction();

    DATABASE_ACTION_TYPE GetType() const override
    {
        return DATABASE_ACTION_TYPE::CollectionReorder;
    }

    const auto& GetNewOrder() const
    {
        return NewOrder;
    }

    const auto& GetOldOrder() const
    {
        return OldOrder;
    }

    auto GetTargetCollection() const
    {
        return TargetCollection;
    }


    std::string GenerateDescription() const override;
    std::vector<std::shared_ptr<ResourceWithPreview>> LoadPreviewItems(
        int max = 10) const override;

    bool SupportsEditing() const override
    {
        return true;
    }

    void OpenEditingWindow() override;

protected:
    void _OnPurged() override {}

    void SetOldOrder(const std::vector<std::tuple<DBID, int64_t>>& oldorder)
    {
        OldOrder = oldorder;
        OnMarkDirty();
    }

private:
    void _Redo() override;
    void _Undo() override;
    void _SerializeCustomData(Json::Value& value) const override;

private:
    DBID TargetCollection;

    //! The new order
    std::vector<DBID> NewOrder;

    //! The stored old order for undo purposes
    std::vector<std::tuple<DBID, int64_t>> OldOrder;
};


//! \brief NetGallery was deleted (marked deleted)
class NetGalleryDeleteAction final : public DatabaseAction {
    friend Database;
    friend std::shared_ptr<DatabaseAction> DatabaseAction::Create(
        Database& db, DatabaseLockT& dblock, PreparedStatement& statement, DBID id);

protected:
    NetGalleryDeleteAction(
        DBID id, Database& from, bool performed, const std::string& customdata);

public:
    //! This is public just to make std::make_shared work
    //! \protected
    NetGalleryDeleteAction(DBID resource);
    ~NetGalleryDeleteAction();

    DATABASE_ACTION_TYPE GetType() const override
    {
        return DATABASE_ACTION_TYPE::NetGalleryDelete;
    }

    const auto& GetResourceToDelete() const
    {
        return ResourceToDelete;
    }

    std::string GenerateDescription() const override;

protected:
    void _OnPurged() override;

private:
    void _Redo() override;
    void _Undo() override;
    void _SerializeCustomData(Json::Value& value) const override;

private:
    DBID ResourceToDelete;
};

} // namespace DV
