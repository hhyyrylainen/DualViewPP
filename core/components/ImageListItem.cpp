// ------------------------------------ //
#include "ImageListItem.h"

#include "core/resources/Image.h"

using namespace DV;
// ------------------------------------ //
ImageListItem::ImageListItem(std::shared_ptr<Image> shownImage /*= nullptr*/) :
    ListItem(shownImage, shownImage ? shownImage->GetName() : "")
{
    
    
}
