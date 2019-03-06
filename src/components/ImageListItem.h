#pragma once

#include "ListItem.h"

#include <memory>

namespace DV{

class Image;
class Collection;

//! \brief Widget type for ImagePreview
class ImageListItem : public ListItem{
public:

    ImageListItem(const std::shared_ptr<ItemSelectable> &selectable,
        std::shared_ptr<Image> shownimage = nullptr);

    //! \brief Sets the shown image
    void SetImage(std::shared_ptr<Image> image);

    //! \brief Sets collection for browsing
    //! \note This doesn't make the preview widget's default image scrollable
    void SetCollection(std::shared_ptr<Collection> collection);

private:

    std::shared_ptr<Image> CurrentImage;
};

}
    
