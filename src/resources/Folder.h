#pragma once

#include "DatabaseResource.h"
#include "ResourceWithPreview.h"

#include <string>

namespace DV {

class PreparedStatement;
class FolderListItem;

class Folder : public DatabaseResource,
               public ResourceWithPreview,
               public std::enable_shared_from_this<Folder> {
public:
    //! \brief Database load function
    Folder(Database& db, DatabaseLockT& dblock, PreparedStatement& statement, int64_t id);

    ~Folder();

    //! \brief Renames this collection
    //! \returns True on success, false if the new name conflicts
    bool Rename(const std::string& newName);

    const auto GetName() const
    {
        return Name;
    }

    const auto GetIsPrivate() const
    {
        return IsPrivate;
    }

    //! \brief Returns true if this is the root folder
    bool IsRoot() const;

    //! \brief Returns true if the folders are the same
    bool operator==(const Folder& other) const;

    // Implementation of ResourceWithPreview
    std::shared_ptr<ListItem> CreateListItem(
        const std::shared_ptr<ItemSelectable>& selectable) override;
    bool IsSame(const ResourceWithPreview& other) override;
    bool UpdateWidgetWithValues(ListItem& control) override;

protected:
    // DatabaseResource implementation
    void _DoSave(Database& db) override;
    void _DoSave(Database& db, DatabaseLockT& dbLock) override;

    //! \brief Fills a widget with this resource
    void _FillWidget(FolderListItem& widget);

protected:
    std::string Name;

    bool IsPrivate = false;
};

} // namespace DV
