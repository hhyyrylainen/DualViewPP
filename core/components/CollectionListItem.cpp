// ------------------------------------ //
#include "CollectionListItem.h"

#include "core/resources/Collection.h"

#include "DualView.h"
#include "CacheManager.h"

using namespace DV;
// ------------------------------------ //
CollectionListItem::CollectionListItem(const std::shared_ptr<ItemSelectable> &selectable,
    std::shared_ptr<Collection> showncollection /*= nullptr*/) :
    ListItem(showncollection ? showncollection->GetPreviewIcon() : nullptr,
        showncollection ? showncollection->GetName() : "",
        selectable, true),
    CurrentCollection(showncollection),

    ItemView("_View", true),
    ItemAddToFolder("_Add To Folder", true),
    ItemRemoveFromFolders("_Remove From Folders...", true)
{
    ImageIcon.SetBackground(DualView::Get().GetCacheManager().GetCollectionIcon());

    // Construct popup menu //
    ContextMenu.set_accel_group(Gtk::AccelGroup::create());
    
    ContextMenu.append(ItemView);
    ContextMenu.append(ItemSeparator1);
    ContextMenu.append(ItemAddToFolder);
    ContextMenu.append(ItemRemoveFromFolders);
    
    ContextMenu.show_all_children();

    ContextMenu.set_accel_path("<CollectionList-Item>/Right");

    ItemView.signal_activate().connect(sigc::mem_fun(*this, &CollectionListItem::_DoPopup));

    // Set scroll
    if(showncollection)
        ImageIcon.SetImageList(showncollection);
}
// ------------------------------------ //
void CollectionListItem::SetCollection(std::shared_ptr<Collection> collection){

    CurrentCollection = collection;

    _SetImage(collection->GetPreviewIcon());
    _SetName(collection->GetName());
    ImageIcon.SetImageList(collection);
}
// ------------------------------------ //
void CollectionListItem::_DoPopup(){
    
    DualView::Get().OpenSingleCollectionView(CurrentCollection);
}

bool CollectionListItem::_OnRightClick(GdkEventButton* causedbyevent){

    // Would be nice to use this but it's in gtk 3.22
    //ContextMenu.popup_at_pointer(causedbyevent);
    ContextMenu.popup(causedbyevent->button, causedbyevent->time);
    
    return true;
}
