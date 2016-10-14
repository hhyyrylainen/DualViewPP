// ------------------------------------ //
#include "ImageListItem.h"

#include "core/resources/Image.h"

using namespace DV;
// ------------------------------------ //
ImageListItem::ImageListItem(std::shared_ptr<Image> shownImage /*= nullptr*/) :
    ListItem(shownImage, shownImage ? shownImage->GetName() : "", true, true),
    CurrentImage(shownImage)
{
    
}
// ------------------------------------ //
void ImageListItem::SetImage(std::shared_ptr<Image> image){

    CurrentImage = image;
    _SetImage(image);
    _SetName(image->GetName());
}
