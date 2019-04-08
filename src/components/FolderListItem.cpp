// ------------------------------------ //
#include "FolderListItem.h"

#include "resources/Folder.h"

#include "CacheManager.h"
#include "DualView.h"

using namespace DV;
// ------------------------------------ //
FolderListItem::FolderListItem(const std::shared_ptr<ItemSelectable>& selectable,
    std::shared_ptr<Folder> shownfolder /*= nullptr*/) :
    ListItem(nullptr, shownfolder ? shownfolder->GetName() : "", selectable, false),
    CurrentFolder(shownfolder)
{
    ImageIcon.SetImage(DualView::Get().GetCacheManager().GetFolderAsImage());
    Container.set_homogeneous(true);
}
// ------------------------------------ //
void FolderListItem::SetFolder(std::shared_ptr<Folder> folder)
{
    CurrentFolder = folder;
    _SetName(folder->GetName());
}
// ------------------------------------ //
void FolderListItem::_DoPopup()
{
    if(!Selectable || !Selectable->UsesCustomPopup)
        return;

    // Folder opened //
    Selectable->CustomPopup(*this);
}
// ------------------------------------ //
void FolderListItem::SetItemSize(LIST_ITEM_SIZE newsize)
{
    ListItem::SetItemSize(newsize);

    switch(newsize) {
    case LIST_ITEM_SIZE::NORMAL: {
        Container.set_homogeneous(true);
        break;
    }
    default: {
        Container.set_homogeneous(false);
        break;
    }
    }
}
