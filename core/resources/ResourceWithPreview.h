#pragma once

#include <memory>

namespace DV{

class ListItem;

//! Main class for all the things that can be in SuperContainer
class ResourceWithPreview{
public:
    
    //! \brief Creates a widget of this type with the values in this
    virtual std::shared_ptr<ListItem> CreateListItem() = 0;

    //! \brief Returns true if this and other has the same actual type and the same
    //! member values
    virtual bool IsSame(const ResourceWithPreview &other) = 0;

    //! \brief Updates a control with values in this
    //! \returns True if the widget was of right type and was updated
    virtual bool UpdateWidgetWithValues(ListItem &control) = 0;
};

}
