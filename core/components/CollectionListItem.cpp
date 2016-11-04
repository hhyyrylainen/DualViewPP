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
    CurrentCollection(showncollection)
{
    // TODO: set collection background image
    ImageIcon.SetBackground(DualView::Get().GetCacheManager().GetCollectionIcon());
}
// ------------------------------------ //
void CollectionListItem::SetCollection(std::shared_ptr<Collection> collection){

    CurrentCollection = collection;

    // TODO: update preview image
    _SetImage(collection->GetPreviewIcon());
    _SetName(collection->GetName());
}
// ------------------------------------ //
void CollectionListItem::_DoPopup(){
    
    DualView::Get().OpenSingleCollectionView(CurrentCollection);
}
