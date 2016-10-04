#pragma once

#include "ListItem.h"

#include <memory>

namespace DV{

class Image;

//! \brief Widget type for ImagePreview
class ImageListItem : public ListItem{
public:

    
    ImageListItem(std::shared_ptr<Image> shownImage = nullptr);

    //! \brief Sets the shown image
    void SetImage(std::shared_ptr<Image> image);


private:

    std::shared_ptr<Image> CurrentImage;
};

}
    
