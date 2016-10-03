#pragma once

#include "ListItem.h"

#include <memory>

namespace DV{

class Image;

//! \brief Widget type for ImagePreview
class ImageListItem : public ListItem{
public:

    
    ImageListItem(std::shared_ptr<Image> shownImage = nullptr);

    
};

}
    
