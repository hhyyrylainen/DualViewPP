#pragma once

#include "ListItem.h"

#include <memory>

namespace DV{

class Image;

//! \brief Widget type for ImagePreview
class ImageListItem : public ListItem{
public:

    
    ImageListItem(const ItemSelectable &selectable,
        std::shared_ptr<Image> shownimage = nullptr);

    //! \brief Sets the shown image
    void SetImage(std::shared_ptr<Image> image);


private:

    std::shared_ptr<Image> CurrentImage;
};

}
    
