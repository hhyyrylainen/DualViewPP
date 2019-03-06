// ------------------------------------ //
#include "Folder.h"

#include "Database.h"
#include "PreparedStatement.h"
#include "components/FolderListItem.h"

using namespace DV;
// ------------------------------------ //

Folder::Folder(Database& db, Lock& dblock, PreparedStatement& statement, int64_t id) :

    DatabaseResource(id, db)
{
    // Load properties //
    CheckRowID(statement, 1, "name");
    CheckRowID(statement, 2, "is_private");

    Name = statement.GetColumnAsString(1);
    IsPrivate = statement.GetColumnAsBool(2);
}

Folder::~Folder()
{
    DBResourceDestruct();
}
// ------------------------------------ //
void Folder::_DoSave(Database& db)
{
    db.UpdateFolder(*this);
}

bool Folder::operator==(const Folder& other) const
{
    if(static_cast<const DatabaseResource&>(*this) ==
        static_cast<const DatabaseResource&>(other)) {
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
std::shared_ptr<ListItem> Folder::CreateListItem(
    const std::shared_ptr<ItemSelectable>& selectable)
{
    auto widget = std::make_shared<FolderListItem>(selectable);

    _FillWidget(*widget);

    return widget;
}

bool Folder::IsSame(const ResourceWithPreview& other)
{
    auto* asThis = dynamic_cast<const Folder*>(&other);

    if(!asThis)
        return false;

    // Check is the folder same //
    return *this == *asThis;
}

bool Folder::UpdateWidgetWithValues(ListItem& control)
{
    auto* asOurType = dynamic_cast<FolderListItem*>(&control);

    if(!asOurType)
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
