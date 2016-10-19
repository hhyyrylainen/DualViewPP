// ------------------------------------ //
#include "CollectionListItem.h"

#include "core/resources/Collection.h"

using namespace DV;
// ------------------------------------ //
CollectionListItem::CollectionListItem(const ItemSelectable &selectable,
    std::shared_ptr<Collection> showncollection /*= nullptr*/) :
    ListItem(/*showncollection->GetPreviewIcon()*/nullptr,
        showncollection ? showncollection->GetName() : "",
        selectable, true),
    CurrentCollection(showncollection)
{
    // TODO: set collection background image
}
// ------------------------------------ //
void CollectionListItem::SetCollection(std::shared_ptr<Collection> collection){

    CurrentCollection = collection;

    // TODO: update preview image
    //_SetImage(collection->GetPreviewIcon());
    _SetName(collection->GetName());
}
