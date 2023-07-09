// ------------------------------------ //
#include "Folder.h"

#include "components/FolderListItem.h"

#include "Database.h"
#include "PreparedStatement.h"

using namespace DV;

// ------------------------------------ //
Folder::Folder(Database& db, DatabaseLockT& dblock, PreparedStatement& statement, int64_t id) :

    DatabaseResource(id, db)
{
    // Load properties //
    CheckRowID(statement, 1, "name");
    CheckRowID(statement, 2, "is_private");
    CheckRowID(statement, 3, "deleted");

    Name = statement.GetColumnAsString(1);
    IsPrivate = statement.GetColumnAsBool(2);

    Deleted = statement.GetColumnAsOptionalBool(3);
}

Folder::~Folder()
{
    DBResourceDestruct();
}

// ------------------------------------ //
void Folder::_DoSave(Database& db)
{
    GUARD_LOCK_OTHER(db);
    db.UpdateFolder(guard, *this);
}

void Folder::_DoSave(Database& db, DatabaseLockT& dbLock)
{
    db.UpdateFolder(dbLock, *this);
}

// ------------------------------------ //
bool Folder::Rename(const std::string& newName)
{
    if (Name == newName)
        return true;

    if (newName.empty())
        return false;

    if (!IsInDatabase())
    {
        Name = newName;
        OnMarkDirty();
        return true;
    }

    {
        GUARD_LOCK_OTHER(InDatabase);

        if (InDatabase->SelectFirstParentFolderWithChildFolderNamed(guard, *this, newName))
            return false;

        const auto oldName = std::move(Name);
        Name = newName;

        try
        {
            if (!InDatabase->UpdateFolder(guard, *this))
            {
                // Old name is restored on failure
                Name = oldName;
                return false;
            }
        }
        catch (const InvalidSQL& e)
        {
            LOG_INFO("Failed to rename folder due to SQL error:");
            e.PrintToLog();

            Name = oldName;
            return false;
        }
    }

    GUARD_LOCK();
    IsDirty = false;
    NotifyAll(guard);

    return true;
}

// ------------------------------------ //
bool Folder::AddFolder(const std::shared_ptr<Folder>& otherFolder)
{
    if (!otherFolder || !otherFolder->IsInDatabase() || !IsInDatabase())
        return false;

    GUARD_LOCK_OTHER(InDatabase);

    const auto conflict = InDatabase->SelectFolderByNameAndParent(guard, otherFolder->GetName(), *this);

    if (conflict)
        return false;

    InDatabase->InsertFolderToFolder(guard, *otherFolder, *this);
    return true;
}

bool Folder::RemoveFolder(std::shared_ptr<Folder> otherFolder)
{
    if (!otherFolder || !otherFolder->IsInDatabase() || !IsInDatabase())
        return false;

    GUARD_LOCK_OTHER(InDatabase);

    if (!InDatabase->DeleteFolderFromFolder(guard, *otherFolder, *this))
        return false;

    InDatabase->InsertToRootFolderIfInNoFolders(guard, *otherFolder);
    return true;
}

// ------------------------------------ //
bool Folder::operator==(const Folder& other) const
{
    if (static_cast<const DatabaseResource&>(*this) == static_cast<const DatabaseResource&>(other))
    {
        return true;
    }

    return Name == other.Name;
}

bool Folder::IsRoot() const
{
    return ID == DATABASE_ROOT_FOLDER_ID;
}

// ------------------------------------ //
// Implementation of ResourceWithPreview
std::shared_ptr<ListItem> Folder::CreateListItem(const std::shared_ptr<ItemSelectable>& selectable)
{
    auto widget = std::make_shared<FolderListItem>(selectable);

    _FillWidget(*widget);

    return widget;
}

bool Folder::IsSame(const ResourceWithPreview& other)
{
    auto* asThis = dynamic_cast<const Folder*>(&other);

    if (!asThis)
        return false;

    // Check is the folder same //
    return *this == *asThis;
}

bool Folder::UpdateWidgetWithValues(ListItem& control)
{
    auto* asOurType = dynamic_cast<FolderListItem*>(&control);

    if (!asOurType)
        return false;

    // Update the properties //
    _FillWidget(*asOurType);
    return true;
}

void Folder::_FillWidget(FolderListItem& widget)
{
    widget.SetFolder(shared_from_this());
    widget.Deselect();
}
