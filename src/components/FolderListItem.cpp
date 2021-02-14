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
    CurrentFolder(shownfolder), ItemAddToFolder("_Add To Folder", true),
    ItemRemoveFromFolders("_Remove From Folders...", true), ItemRename("Re_name", true)
{
    ImageIcon.SetImage(DualView::Get().GetCacheManager().GetFolderAsImage());
    Container.set_homogeneous(true);

    // Construct popup menu //
    ContextMenu.set_accel_group(Gtk::AccelGroup::create());

    ContextMenu.append(ItemAddToFolder);
    ContextMenu.append(ItemRemoveFromFolders);
    ContextMenu.append(ItemSeparator);
    ContextMenu.append(ItemRename);

    ContextMenu.attach_to_widget(*this);
    ContextMenu.show_all_children();

    ContextMenu.set_accel_path("<CollectionList-Item>/Right");

    ItemAddToFolder.signal_activate().connect(
        sigc::mem_fun(*this, &FolderListItem::_OpenAddToFolder));

    ItemRemoveFromFolders.signal_activate().connect(
        sigc::mem_fun(*this, &FolderListItem::_OpenRemoveFromFolders));

    ItemRename.signal_activate().connect(sigc::mem_fun(*this, &FolderListItem::_OpenRename));
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
// ------------------------------------ //
bool FolderListItem::_OnRightClick(GdkEventButton* causedbyevent)
{
    ContextMenu.popup_at_pointer(nullptr);

    return true;
}
// ------------------------------------ //
void FolderListItem::_OpenRemoveFromFolders()
{
    DualView::Get().OpenRemoveFromFolders(CurrentFolder);
}

void FolderListItem::_OpenAddToFolder()
{
    DualView::Get().OpenAddToFolder(CurrentFolder);
}

void FolderListItem::_OpenRename()
{
    DualView::Get().OpenFolderRename(
        CurrentFolder, dynamic_cast<Gtk::Window*>(get_toplevel()));
}
