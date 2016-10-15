// ------------------------------------ //
#include "ImageListItem.h"

#include "core/resources/Image.h"

using namespace DV;
// ------------------------------------ //
ImageListItem::ImageListItem(const ItemSelectable &selectable,
    std::shared_ptr<Image> shownImage /*= nullptr*/) :
    ListItem(shownImage, shownImage ? shownImage->GetName() : "", selectable, true),
    CurrentImage(shownImage)
{
    
}
// ------------------------------------ //
void ImageListItem::SetImage(std::shared_ptr<Image> image){

    CurrentImage = image;
    _SetImage(image);
    _SetName(image->GetName());
}
