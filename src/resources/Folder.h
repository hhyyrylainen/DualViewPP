#pragma once

#include <string>

#include "DatabaseResource.h"
#include "Exceptions.h"
#include "ResourceWithPreview.h"

namespace DV
{
class PreparedStatement;
class FolderListItem;

class Folder : public DatabaseResource,
               public ResourceWithPreview,
               public std::enable_shared_from_this<Folder>
{
    friend Database;
    friend MaintenanceTools;

public:
    //! \brief Database load function
    Folder(Database& db, DatabaseLockT& dblock, PreparedStatement& statement, int64_t id);

    ~Folder();

    //! \brief Renames this collection
    //! \returns True on success, false if the new name conflicts
    bool Rename(const std::string& newName);

    //! \brief Adds a new folder into this folder
    //! \returns True on success, false if name is duplicate, folder is already in here or some
    //! other problem
    bool AddFolder(const std::shared_ptr<Folder>& otherFolder);

    //! \brief Removes a new folder from this folder
    //! \returns True if removed
    bool RemoveFolder(std::shared_ptr<Folder> otherFolder);

    const auto GetName() const
    {
        return Name;
    }

    const auto GetIsPrivate() const
    {
        return IsPrivate;
    }

    bool IsDeleted() const
    {
        return Deleted;
    }

    //! \brief Returns true if this is the root folder
    bool IsRoot() const;

    //! \brief Returns true if the folders are the same
    bool operator==(const Folder& other) const;

    // Implementation of ResourceWithPreview
    std::shared_ptr<ListItem> CreateListItem(const std::shared_ptr<ItemSelectable>& selectable) override;
    bool IsSame(const ResourceWithPreview& other) override;
    bool UpdateWidgetWithValues(ListItem& control) override;

protected:
    // DatabaseResource implementation
    void _DoSave(Database& db) override;
    void _DoSave(Database& db, DatabaseLockT& dbLock) override;

    //! \brief Fills a widget with this resource
    void _FillWidget(FolderListItem& widget);

    //! Called from Database
    void _UpdateDeletedStatus(bool deleted)
    {
        Deleted = deleted;

        GUARD_LOCK();
        NotifyAll(guard);
    }

    void ForceUnDeleteToFixMissingAction()
    {
        if (!Deleted)
            throw Leviathan::Exception("This needs to be in deleted state to call this fix missing action");

        Deleted = false;
    }

protected:
    std::string Name;

    bool IsPrivate = false;

    //! If true deleted (or marked deleted) from the database
    bool Deleted = false;
};

} // namespace DV
