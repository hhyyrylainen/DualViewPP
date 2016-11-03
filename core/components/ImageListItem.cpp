// ------------------------------------ //
#include "ImageListItem.h"

#include "core/resources/Image.h"

using namespace DV;
// ------------------------------------ //
ImageListItem::ImageListItem(const std::shared_ptr<ItemSelectable> &selectable,
    std::shared_ptr<Image> shownimage /*= nullptr*/) :
    ListItem(shownimage, shownimage ? shownimage->GetName() : "", selectable, true),
    CurrentImage(shownimage)
{
    
}
// ------------------------------------ //
void ImageListItem::SetImage(std::shared_ptr<Image> image){

    CurrentImage = image;
    _SetImage(image);
    _SetName(image->GetName());
}
