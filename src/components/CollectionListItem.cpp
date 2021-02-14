// ------------------------------------ //
#include "CollectionListItem.h"

#include "resources/Collection.h"

#include "CacheManager.h"
#include "DualView.h"

using namespace DV;
// ------------------------------------ //
CollectionListItem::CollectionListItem(const std::shared_ptr<ItemSelectable>& selectable,
    std::shared_ptr<Collection> showncollection /*= nullptr*/) :
    ListItem(showncollection ? showncollection->GetPreviewIcon() : nullptr,
        showncollection ? showncollection->GetName() : "", selectable, true),
    CurrentCollection(showncollection), ItemView("_View", true),
    ItemAddToFolder("_Add To Folder", true),
    ItemRemoveFromFolders("_Remove From Folders...", true), ItemReorder("Re_order", true)
{
    ImageIcon.SetBackground(DualView::Get().GetCacheManager().GetCollectionIcon());

    // Construct popup menu //
    ContextMenu.set_accel_group(Gtk::AccelGroup::create());

    ContextMenu.append(ItemView);
    ContextMenu.append(ItemSeparator1);
    ContextMenu.append(ItemAddToFolder);
    ContextMenu.append(ItemRemoveFromFolders);
    ContextMenu.append(ItemSeparator2);
    ContextMenu.append(ItemReorder);

    ContextMenu.attach_to_widget(*this);
    ContextMenu.show_all_children();

    ContextMenu.set_accel_path("<CollectionList-Item>/Right");

    ItemView.signal_activate().connect(sigc::mem_fun(*this, &CollectionListItem::_DoPopup));

    ItemAddToFolder.signal_activate().connect(
        sigc::mem_fun(*this, &CollectionListItem::_OpenAddToFolder));

    ItemRemoveFromFolders.signal_activate().connect(
        sigc::mem_fun(*this, &CollectionListItem::_OpenRemoveFromFolders));

    ItemReorder.signal_activate().connect(
        sigc::mem_fun(*this, &CollectionListItem::_OpenReorderView));


    // Set scroll
    if(showncollection)
        ImageIcon.SetImageList(showncollection);
}
// ------------------------------------ //
void CollectionListItem::SetCollection(std::shared_ptr<Collection> collection)
{
    bool isSwitch = CurrentCollection != nullptr;

    CurrentCollection = collection;

    // Update the item right away to make navigation less confusing
    _SetImage(nullptr, isSwitch);
    _SetName("Loading...");

    auto alive = GetAliveMarker();

    DualView::Get().QueueDBThreadFunction([=]() {
        auto preview = collection->GetPreviewIcon();

        DualView::Get().InvokeFunction([=]() {
            INVOKE_CHECK_ALIVE_MARKER(alive);
            _SetImage(preview, false);
            _SetName(collection->GetName());
            ImageIcon.SetImageList(collection);
        });
    });
}
// ------------------------------------ //
void CollectionListItem::_DoPopup()
{
    DualView::Get().OpenSingleCollectionView(CurrentCollection);
}

bool CollectionListItem::_OnRightClick(GdkEventButton* causedbyevent)
{
    ContextMenu.popup_at_pointer(nullptr);
    // ContextMenu.popup_at_widget(this, Gdk::GRAVITY_CENTER, Gdk::GRAVITY_NORTH_EAST,
    // nullptr);

    return true;
}
// ------------------------------------ //
void CollectionListItem::_OpenRemoveFromFolders()
{
    DualView::Get().OpenRemoveFromFolders(CurrentCollection);
}

void CollectionListItem::_OpenAddToFolder()
{
    DualView::Get().OpenAddToFolder(CurrentCollection);
}

void CollectionListItem::_OpenReorderView()
{
    DualView::Get().OpenReorder(CurrentCollection);
}
