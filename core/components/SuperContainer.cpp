// ------------------------------------ //
#include "SuperContainer.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //

//#define SUPERCONTAINER_RESIZE_REFLOW_CHECK_ONLY_FIRST_ROW

SuperContainer::SuperContainer() :
    Gtk::ScrolledWindow(), View(get_hadjustment(), get_vadjustment())
{
    _CommonCtor();
}

SuperContainer::SuperContainer(_GtkScrolledWindow* widget, Glib::RefPtr<Gtk::Builder> builder)
    : Gtk::ScrolledWindow(widget), View(get_hadjustment(), get_vadjustment())
{
    _CommonCtor();
}

void SuperContainer::_CommonCtor(){

    add(View);
    View.add(Container);
    View.show();
    Container.show();

    signal_size_allocate().connect(sigc::mem_fun(*this, &SuperContainer::_OnResize));
}

SuperContainer::~SuperContainer(){

    Clear();
}
// ------------------------------------ //
void SuperContainer::Clear(){

    // This will delete all the widgets //
    Positions.clear();
    LayoutDirty = false;
}

void SuperContainer::UpdatePositioning(){

    if(!LayoutDirty)
        return;

    LayoutDirty = false;

    if(!Positions.empty())
        return;
    
    WidestRow = 0;

    int32_t CurrentRow = SUPERCONTAINER_MARGIN;
    int32_t CurrentY = Positions.front().Y;

    for(auto& position : Positions){

        if(position.Y != CurrentY){

            // Row changed //
            if(WidestRow < CurrentRow)
                WidestRow = CurrentRow;

            CurrentRow = SUPERCONTAINER_MARGIN;
            CurrentY = position.Y;
        }

        CurrentRow += position.Width + SUPERCONTAINER_PADDING;

        auto& element = position.WidgetToPosition;

        Container.move(*element->Widget, position.X, position.Y);
    }

    // Last row needs to be included, too //
    if(WidestRow < CurrentRow)
        WidestRow = CurrentRow;
}

size_t SuperContainer::CountRows() const{

    size_t count = 0;

    int32_t CurrentY = -1;

    for(auto& position : Positions){

        if(position.Y != CurrentY){
            
            ++count;
            CurrentY = position.Y;
        }
    }
    
    return count;
}
// ------------------------------------ //
void SuperContainer::Reflow(size_t index){

    if(Positions.empty() || index >= Positions.size())
        return;

    LayoutDirty = true;

    // The first one doesn't have a previous position //
    if(index == 0){

        _PositionGridPosition(Positions.front(), nullptr);
        ++index;
    }

    for(size_t i = index; i < Positions.size(); ++i){

        _PositionGridPosition(Positions.front(), &Positions[i - 1]);
    }
}

void SuperContainer::_PositionGridPosition(GridPosition& current,
    const GridPosition* previous) const
{
    // First is at fixed position //
    if(previous == nullptr){

        current.X = SUPERCONTAINER_MARGIN;
        current.Y = SUPERCONTAINER_MARGIN;
        return;
    } 

    // Check does it fit on the line //

    if(previous->X + previous->Width + SUPERCONTAINER_PADDING + current.Width <= get_width()){

        // It fits on the line //
        
    }
    
        
}

SuperContainer::GridPosition& SuperContainer::_AddNewGridPosition(
    int32_t width, int32_t height)
{
    GridPosition pos;
    pos.Width = width;
    pos.Height = height;

    _PositionGridPosition(pos, &Positions.back());
    
    Positions.push_back(pos);

    return Positions.back();
}
// ------------------------------------ //
void SuperContainer::_SetWidgetSize(ListItem &widget){

    int width_min, width_natural;
    int height_min, height_natural;

    widget.show();
        
    widget.get_preferred_width(width_min, width_natural);
    widget.get_preferred_height_for_width(width_natural, height_min, height_natural);

    widget.set_size_request(width_natural, height_natural);
}
// ------------------------------------ //
void SuperContainer::_SetKeepFalse(){

    for(auto& position : Positions){

        position.WidgetToPosition->Keep = false;
    }
}

void SuperContainer::_RemoveElementsNotMarkedKeep(){

    size_t reflowStart = Positions.size();

    for(size_t i = 0; i < Positions.size(); ){

        auto& current = Positions[i];

        // If the current position has no widget try to get the next widget, or end //
        if(!current.WidgetToPosition){

            if(i + 1 < Positions.size()){

                // Swap with the next one //
                if(Positions[i].SwapWidgets(Positions[i + 1])){

                    if(reflowStart > i)
                        reflowStart = i;
                }
            }

            // If still empty there are no more widgets to process
            if(!current.WidgetToPosition)
                break;
        }
        
        if(!Positions[i].WidgetToPosition->Keep){

            LayoutDirty = true;

            // Remove this widget //
            Container.remove(*current.WidgetToPosition->Widget);
            current.WidgetToPosition.reset();

            // On the next iteration the next widget will be moved to this position
            
        } else {

            ++i;
        }
    }

    if(reflowStart < Positions.size()){
        
        // Need to reflow //
        Reflow(reflowStart);
    }
}
// ------------------------------------ //
void SuperContainer::_RemoveWidget(size_t index){

    if(index >= Positions.size())
        throw Leviathan::InvalidArgument("index out of range");

    LayoutDirty = true;

    size_t reflowStart = Positions.size();

    Container.remove(*Positions[index].WidgetToPosition->Widget);
    Positions[index].WidgetToPosition.reset();

    // Move forward all the widgets //
    for(size_t i = index; i + 1 < Positions.size(); ++i){

        if(Positions[i].SwapWidgets(Positions[i + 1])){

            if(reflowStart > i)
                reflowStart = i;
        }
    }

    if(reflowStart < Positions.size()){
        
        // Need to reflow //
        Reflow(reflowStart);
    }
}

void SuperContainer::_SetWidget(size_t index, std::shared_ptr<Element> widget,
    bool autoreplace)
{
    if(index >= Positions.size())
        throw Leviathan::InvalidArgument("index out of range");

    if(Positions[index].WidgetToPosition && !autoreplace){

        throw Leviathan::InvalidState("index is not empty and no autoreplace specified");
        
    } else {

        // Remove the current one
        Container.remove(*Positions[index].WidgetToPosition->Widget);
    }

    // Initialize a size for the widget
    _SetWidgetSize(*widget->Widget);

    // Set it //
    if(Positions[index].SetNewWidget(widget)){

        // Do a reflow //
        Reflow(index);
    }
}
// ------------------------------------ //
void SuperContainer::_PushBackWidgets(size_t index){

    if(Positions.empty())
        return;
    
    LayoutDirty = true;

    size_t reflowStart = Positions.size();

    // Create a new position and then pull back widgets until index is reached and then
    // stop

    // We can skip adding if the last is empty //
    if(Positions.back().WidgetToPosition)
        _AddNewGridPosition(Positions.back().Width, Positions.back().Height);

    for(size_t i = Positions.size() - 1; i > index; --i){

        // Swap pointers around, the current index is always empty so the empty
        // spot will propagate to index
        // Also i - 1 cannot be out of range as the smallest index 0 wouldn't enter
        // this loop

        if(Positions[i].SwapWidgets(Positions[i - 1])){

            if(reflowStart > i)
                reflowStart = i;
        }
    }

    if(reflowStart < Positions.size()){
        
        // Need to reflow //
        Reflow(reflowStart);
    }
}

void SuperContainer::_AddWidgetToEnd(std::shared_ptr<ResourceWithPreview> item){

    // Create the widget //
    auto element = std::make_shared<Element>(item);

    // Initialize a size for the widget
    _SetWidgetSize(*element->Widget);

    // Find the first empty spot //
    for(size_t i = 0; i < Positions.size(); ++i){

        if(!Positions[i].WidgetToPosition){

            if(Positions[i].SetNewWidget(element)){

                // Do a reflow //
                Reflow(i);
            }
            
            return;
        }
    }

    // No empty spots, create a new one //
    GridPosition& pos = _AddNewGridPosition(element->Widget->get_width(),
        element->Widget->get_height());

    pos.WidgetToPosition = element;
}
// ------------------------------------ //
// Callbacks
void SuperContainer::_OnResize(Gtk::Allocation &allocation){

    if(Positions.empty())
        return;

    bool reflow = false;

    if(allocation.get_width() < WidestRow){

        // Rows don't fit anymore //
        reflow = true;
        
    } else {

        // Check would wider rows fit //
        int32_t CurrentRow = 0;
        int32_t CurrentY = Positions.front().Y;

        for(auto& position : Positions){

            if(position.Y != CurrentY){

                // Row changed //

                if(SUPERCONTAINER_MARGIN + CurrentRow + SUPERCONTAINER_PADDING + position.Width
                    < allocation.get_width())
                {
                    // The previous row (this is the first position of the first row) could
                    // now fit this widget
                    reflow = true;
                    break;
                }

                CurrentRow = 0;
                CurrentY = position.Y;

                // Break if only checking first row
            #ifdef SUPERCONTAINER_RESIZE_REFLOW_CHECK_ONLY_FIRST_ROW
                break;
            #endif
            }

            CurrentRow += position.Width;
        }
    }

    if(reflow){

        Reflow(0);
        UpdatePositioning();
    }
}

// ------------------------------------ //
// GridPosition
bool SuperContainer::GridPosition::SetNewWidget(std::shared_ptr<Element> widget){

    const auto newWidth = widget->Widget->get_width();
    const auto newHeight = widget->Widget->get_height();

    WidgetToPosition = widget;

    if(newWidth != Width && newHeight != Height){

        Width = newWidth;
        Height = newHeight;
        return true;
    }

    return false;
}

bool SuperContainer::GridPosition::SwapWidgets(GridPosition &other){

    WidgetToPosition.swap(other.WidgetToPosition);

    if(Width != other.Width || Height != other.Height){

        Width = other.Width;
        Height = other.Height;
        return true;
    }

    return false;
}
