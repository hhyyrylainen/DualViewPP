#pragma once

#include "ListItem.h"

#include <memory>

namespace DV{

class Collection;

//! \brief Widget type for CollectionPreview
class CollectionListItem : public ListItem{
public:

    
    CollectionListItem(const ItemSelectable &selectable,
        std::shared_ptr<Collection> showncollection = nullptr);

    //! \brief Sets the shown collection
    void SetCollection(std::shared_ptr<Collection> collection);


private:

    std::shared_ptr<Collection> CurrentCollection;
};

}
    