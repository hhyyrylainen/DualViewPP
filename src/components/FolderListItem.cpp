// ------------------------------------ //
#include "FolderListItem.h"

#include "resources/DatabaseAction.h"
#include "resources/Folder.h"

#include "CacheManager.h"
#include "Database.h"
#include "DualView.h"

using namespace DV;
// ------------------------------------ //
FolderListItem::FolderListItem(const std::shared_ptr<ItemSelectable>& selectable,
    std::shared_ptr<Folder> shownfolder /*= nullptr*/) :
    ListItem(nullptr, shownfolder ? shownfolder->GetName() : "", selectable, false),
    CurrentFolder(shownfolder), ItemAddToFolder("_Add To Folder", true),
    ItemRemoveFromFolders("_Remove From Folders...", true), ItemRename("Re_name", true),
    ItemDelete("_Delete", true)
{
    ImageIcon.SetImage(DualView::Get().GetCacheManager().GetFolderAsImage());
    Container.set_homogeneous(true);

    // Construct popup menu //
    ContextMenu.set_accel_group(Gtk::AccelGroup::create());

    ContextMenu.append(ItemAddToFolder);
    ContextMenu.append(ItemRemoveFromFolders);
    ContextMenu.append(ItemSeparator);
    ContextMenu.append(ItemRename);
    ContextMenu.append(ItemSeparator2);
    ContextMenu.append(ItemDelete);

    ContextMenu.attach_to_widget(*this);
    ContextMenu.show_all_children();

    ContextMenu.set_accel_path("<CollectionList-Item>/Right");

    ItemAddToFolder.signal_activate().connect(
        sigc::mem_fun(*this, &FolderListItem::_OpenAddToFolder));

    ItemRemoveFromFolders.signal_activate().connect(
        sigc::mem_fun(*this, &FolderListItem::_OpenRemoveFromFolders));

    ItemRename.signal_activate().connect(sigc::mem_fun(*this, &FolderListItem::_OpenRename));

    ItemDelete.signal_activate().connect(sigc::mem_fun(*this, &FolderListItem::_StartDelete));
}
// ------------------------------------ //
void FolderListItem::SetFolder(std::shared_ptr<Folder> folder)
{
    // Become active again (if this was a deleted item, that is now reused)
    if(CurrentFolder != folder)
        set_sensitive(true);

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
// ------------------------------------ //
void FolderListItem::_StartDelete()
{
    const auto folder = CurrentFolder;

    if(!folder)
        return;

    set_sensitive(false);

    auto isalive = GetAliveMarker();



    DualView::Get().QueueDBThreadFunction([this, isalive, folder]() {
        auto& db = DualView::Get().GetDatabase();

        int totalItems = 0;
        int totalAddedToRoot = 0;

        {
            GUARD_LOCK_OTHER(db);

            // TODO: this is a bit dirty as we only really care about the count so this creates
            // a bunch of extra temporary objects
            totalItems += db.SelectFoldersInFolder(guard, *folder).size();
            totalAddedToRoot += db.SelectFoldersOnlyInFolder(guard, *folder).size();
            totalItems += db.SelectCollectionsInFolder(guard, *folder).size();
            totalAddedToRoot += db.SelectCollectionsOnlyInFolder(guard, *folder).size();
        }

        DualView::Get().InvokeFunction([this, isalive, totalItems, totalAddedToRoot]() {
            INVOKE_CHECK_ALIVE_MARKER(isalive);

            _OnDeleteInfoReady(totalItems, totalAddedToRoot);
        });
    });
}

void FolderListItem::_OnDeleteInfoReady(int totalContained, int wouldBeAddedToRoot)
{
    if(totalContained < 1 && wouldBeAddedToRoot < 1) {
        // Empty folder, just delete without nagging
        _DeleteTheFolder();
        return;
    }

    auto window = dynamic_cast<Gtk::Window*>(get_toplevel());

    if(!window) {
        LOG_ERROR("FolderListItem not contained in a Window, can't show dialog");
        return;
    }

    auto dialog = Gtk::MessageDialog(*window, "Delete non-empty folder?", false,
        Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);

    dialog.set_secondary_text(
        "The folder to be deleted contains " + std::to_string(totalContained) +
        " item(s), out of which " + std::to_string(wouldBeAddedToRoot) +
        " are only in this folder, and would be moved to the root folder. Delete anyway?");

    int result = dialog.run();

    if(result != Gtk::RESPONSE_YES) {
        set_sensitive(true);
        return;
    }

    _DeleteTheFolder();
}

void FolderListItem::_OnDeleteFailed(const std::string& message)
{
    set_sensitive(true);

    auto window = dynamic_cast<Gtk::Window*>(get_toplevel());

    if(!window) {
        LOG_ERROR("FolderListItem not contained in a Window, can't show dialog");
        return;
    }

    auto dialog = Gtk::MessageDialog(*window, "Failed to delete the folder", false,
        Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);

    dialog.set_secondary_text(
        "Deleting the folder \"" + CurrentFolder->GetName() + "\" failed. Error: " + message);

    dialog.run();
}

void FolderListItem::_DeleteTheFolder()
{
    if(!CurrentFolder || CurrentFolder->IsDeleted())
        return;

    try {
        auto action = DualView::Get().GetDatabase().DeleteFolder(*CurrentFolder);

        if(!action->IsPerformed())
            throw Exception("Delete action failed to be performed");

    } catch(const Exception& e) {
        _OnDeleteFailed(e.what());
    }
}
