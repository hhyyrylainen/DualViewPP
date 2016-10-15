#pragma once

#include <memory>
#include <functional>

namespace DV{

class ListItem;
struct ItemSelectable;

//! Main class for all the things that can be in SuperContainer
class ResourceWithPreview{
public:
    
    //! \brief Creates a widget of this type with the values in this
    virtual std::shared_ptr<ListItem> CreateListItem(const ItemSelectable &selectable) = 0;

    //! \brief Returns true if this and other has the same actual type and the same
    //! member values
    virtual bool IsSame(const ResourceWithPreview &other) = 0;

    //! \brief Updates a control with values in this
    //! \returns True if the widget was of right type and was updated
    //! \todo Update ItemSelectable here
    virtual bool UpdateWidgetWithValues(ListItem &control) = 0;
};

//! \brief Callback holder for selectable ResourceWithPreview
struct ItemSelectable{

    inline ItemSelectable() : Selectable(false){

    }
        
    inline ItemSelectable(std::function<void (ListItem&)> updatecallback) :
        Selectable(true), UpdateCallback(updatecallback)
    {

    }

    bool Selectable;
    std::function<void (ListItem&)> UpdateCallback;
};


}
