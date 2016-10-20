#pragma once

#include "ListItem.h"

#include "core/resources/ResourceWithPreview.h"

#include <gtkmm.h>

#include <list>

namespace DV{

constexpr auto SUPERCONTAINER_MARGIN = 4;
constexpr auto SUPERCONTAINER_PADDING = 2;

//! \brief Holds ListItem derived widgets and arranges them in a scrollable box
//! \todo Add tests for this class
class SuperContainer : public Gtk::ScrolledWindow{
protected:
    
    //! \brief Holds a child widget and some data used for updating in SetShownItems
    struct Element{

        //! \brief Automatically creates the widget from create
        Element(std::shared_ptr<ResourceWithPreview> create, const ItemSelectable &selectable)
            : CreatedFrom(create)
        {

            Widget = CreatedFrom->CreateListItem(selectable);

            if(!Widget)
                throw std::runtime_error("Created Widget is null in Element");
        }
        
        std::shared_ptr<ResourceWithPreview> CreatedFrom;

        //! The size Widget is set to be, This is used because Gtk is very lazy about
        //! calculating the sizes for widgets
        int32_t Width, Height;
        
        std::shared_ptr<ListItem> Widget;

        //! Used to remove removed items when updating the list
        bool Keep = true;
    };

    //! \brief A calculated position to which and element can be added
    struct GridPosition{
    public:

        //! \brief Sets the WidgetToPosition and returns true if the size of this
        //! position was changed
        bool SetNewWidget(std::shared_ptr<Element> widget);

        //! \brief Swaps WidgetToPosition with another grid position
        //! \returns True if sizes have changed
        bool SwapWidgets(GridPosition &other);
        
        //! Topleft coordinates of this position
        int32_t X, Y;

        //! The size that is reserved for the widget, if the widget is larger
        //! all grid positions after this need to recalculated
        int32_t Width, Height;

        //! This is a pointer to allow really cheap swapping when sorting widgets
        std::shared_ptr<Element> WidgetToPosition;
    };

public:

    //! \brief Non-glade constructor
    SuperContainer();
    
    //! \brief Constructor called by glade builder when loading a widget of this type
    SuperContainer(_GtkScrolledWindow* widget, Glib::RefPtr<Gtk::Builder> builder);
    ~SuperContainer();

    //! \brief Replaces all items with the ones in the iterator range
    //!
    //! Will also sort existing items that should be kept
    template<class Iterator>
        void SetShownItems(Iterator begin, Iterator end, const ItemSelectable &selectable =
            ItemSelectable())
    {
        if(Positions.empty()){

            // Update initial width
            LastWidthReflow = get_width();
        }
        
        _SetKeepFalse();

        auto newIndex = begin;
        
        for(size_t i = 0; i < Positions.size() && newIndex != end; ++i, ++newIndex)
        {
            // Empty positions can be just filled //
            if(!Positions[i].WidgetToPosition){

                _SetWidget(i, std::make_shared<Element>(*newIndex, selectable));
                continue;
            }
            
            // Make sure that the order is correct //
            if(Positions[i].WidgetToPosition->CreatedFrom->IsSame(**newIndex)){

                // all is fine //
                Positions[i].WidgetToPosition->Keep = true;
                continue;
            }

            // Need to replace this one //

            // First try to update if the widget is the same type as the new one //
            if((**newIndex).UpdateWidgetWithValues(*Positions[i].WidgetToPosition->Widget)){

                Positions[i].WidgetToPosition->Keep = true;

                // Important to set the resource //
                Positions[i].WidgetToPosition->CreatedFrom = *newIndex;
                continue;
            }

            // Insert a new widget here //
            _PushBackWidgets(i);
            _SetWidget(i, std::make_shared<Element>(*newIndex, selectable));
        }
        
        _RemoveElementsNotMarkedKeep();
        
        // Push new items until all are added //
        while(newIndex != end){

            _AddWidgetToEnd(*newIndex, selectable);
            ++newIndex;
        }

        UpdatePositioning();
    }

    //! \brief Adds a new item at the end, doesn't sort the items
    inline void AddItem(std::shared_ptr<ResourceWithPreview> item,
        const ItemSelectable &selectable = ItemSelectable())
    {
        _AddWidgetToEnd(item, selectable);
        LayoutDirty = true;
        UpdatePositioning();
    }

    //! \brief Returns the currently selected items
    //!
    //! The items will be added to the inserter. Which can be an std::back_inserter or
    //! some container
    template<class TContainer>
        void GetSelectedItems(TContainer &inserter) const
    {
        for(auto& position : Positions){

            // Stop once empty position is reached //
            if(!position.WidgetToPosition)
                break;

            if(position.WidgetToPosition->Widget->IsSelected())
                inserter.push_back(position.WidgetToPosition->CreatedFrom);
        }
    }

    //! \brief Returns the number of items that are selected
    size_t CountSelectedItems() const;

    //! \brief Deselects all items
    void DeselectAllItems();

    //! \brief Selects all items
    void SelectAllItems();

    //! \brief Deselects all items except the specified one
    void DeselectAllExcept(const ListItem* item);


    //! \brief Empties this container completely
    void Clear();

    //! \brief Applies the positioning, will be called whenever Positions is changed
    void UpdatePositioning();

    //! \brief If full positioning update is not needed, this can be called to just update
    //! row widths
    void UpdateRowWidths();

    //! \brief Calculates positions for GridPositions starting at index
    void Reflow(size_t index);

    //! \brief Returns the number of lines the shown items take
    size_t CountRows() const;
    
    //! \brief Returns the width of the widest row, in pixels
    //! \note UpdatePositioning needs to be called before this is updated
    inline auto GetWidestRowWidth() const{

        return WidestRow;
    }

    // Callbacks for contained items //

private:

    void _CommonCtor();
    
protected:

    //! \brief Applies the position of a widget
    inline void _ApplyWidgetPosition(const GridPosition &position){

        auto& element = position.WidgetToPosition;

        if(!element || !element->Widget){
            
            // An empty position //
            return;
        }
        
        Container.move(*element->Widget, position.X, position.Y);
    }

    //! \brief Sets the position of a grid position according to the previous position
    //!
    //! Takes into account the current window size and the height of previous line
    //! \param previous The previous position after which current should be placed
    //! \param previousindex The index of previous in Positions or Positions.size() if
    //! previous is null
    void _PositionGridPosition(GridPosition& current, const GridPosition* const previous,
        size_t previousindex) const;

    //! \brief Creates a new GridPosition and calculates a spot for it
    GridPosition& _AddNewGridPosition(int32_t width, int32_t height);

    //! \brief Sets the size of a new widget
    void _SetWidgetSize(Element &widget);

    //! \brief Sets keep to false on all the widgets
    void _SetKeepFalse();

    //! \brief Removes Elements that aren't marked keep and cascades the
    //! later elements into the empty spots
    void _RemoveElementsNotMarkedKeep();

    //! \brief Removes a widget in Positions at index and moves forward all the later positions
    void _RemoveWidget(size_t index);

    //! \brief Sets the widget at index
    //! \note widget Shouldn't already be in the container
    //! \param autoreplace If true can add to a Position with a widget, if false adding
    //! to a full position does nothing
    void _SetWidget(size_t index, std::shared_ptr<Element> widget, bool autoreplace = false);

    //! \brief Makes position at index empty by pushing back all the later positions' widgets
    void _PushBackWidgets(size_t index);

    //! \brief Adds a new widget to the end
    void _AddWidgetToEnd(std::shared_ptr<ResourceWithPreview> item,
        const ItemSelectable &selectable);


    //! \brief A debug helper, errors if there are duplicates in Positions
    void _CheckPositions() const;
    
    // Callbacks //
    //! \brief Repositions GridPositions if size has changed enough
    //! \note For some reason this doesn't get always called so horizontal scrollbars try
    //! to appear from time to time
    //! \todo Check if `Container.check_resize();` is a performance problem and is there a
    //! alternative fix to forcing position updates when maximizing
    void _OnResize(Gtk::Allocation &allocation);
    
private:
    
    Gtk::Viewport View;
    Gtk::Fixed Container;

    //! True when Positions or widgets have changed and UpdatePositioning should be called
    bool LayoutDirty = true;

    //! The width of the widest row, updated by UpdatePositioning
    int32_t WidestRow = 0;

    //! Used to skip size changes that don't change width
    int LastWidthReflow = 0;

    //! The calculated positions for widgets
    //! All the empty positions must be in a row starting from the last one, so that all
    //! functions that only care about active elements can stop as soon as they encounter
    //! the first null
    std::vector<GridPosition> Positions;
};

}
