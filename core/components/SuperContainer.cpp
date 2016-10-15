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
// ------------------------------------ //
void SuperContainer::UpdatePositioning(){

    if(!LayoutDirty)
        return;

    LayoutDirty = false;
    WidestRow = 0;
    
    if(Positions.empty())
        return;

    int32_t CurrentRow = SUPERCONTAINER_MARGIN;
    int32_t CurrentY = Positions.front().Y;

    for(auto& position : Positions){

        if(position.Y != CurrentY){

            // Row changed //
            if(WidestRow < CurrentRow)
                WidestRow = CurrentRow;

            CurrentRow = position.X;
            CurrentY = position.Y;
        }

        CurrentRow += position.Width + SUPERCONTAINER_PADDING;

        _ApplyWidgetPosition(position);
    }

    // Last row needs to be included, too //
    if(WidestRow < CurrentRow)
        WidestRow = CurrentRow;

    // Add margin
    WidestRow += SUPERCONTAINER_MARGIN;
}

void SuperContainer::UpdateRowWidths(){

    WidestRow = 0;

    int32_t CurrentRow = SUPERCONTAINER_MARGIN;
    int32_t CurrentY = Positions.front().Y;

    for(auto& position : Positions){

        if(position.Y != CurrentY){

            // Row changed //
            if(WidestRow < CurrentRow)
                WidestRow = CurrentRow;

            CurrentRow = position.X;
            CurrentY = position.Y;
        }

        CurrentRow += position.Width + SUPERCONTAINER_PADDING;
    }

    // Last row needs to be included, too //
    if(WidestRow < CurrentRow)
        WidestRow = CurrentRow;

    // Add margin
    WidestRow += SUPERCONTAINER_MARGIN;
}
// ------------------------------------ //
size_t SuperContainer::CountRows() const{

    size_t count = 0;

    int32_t CurrentY = -1;

    for(auto& position : Positions){

        // Stop once empty position is reached //
        if(!position.WidgetToPosition)
            break;

        if(position.Y != CurrentY){
            
            ++count;
            CurrentY = position.Y;
        }
    }
    
    return count;
}

size_t SuperContainer::CountSelectedItems() const{

    size_t count = 0;
    
    for(auto& position : Positions){

        // Stop once empty position is reached //
        if(!position.WidgetToPosition)
            break;

        if(position.WidgetToPosition->Widget->IsSelected())
            ++count;
    }

    return count;
}

void SuperContainer::DeselectAllItems(){

    for(auto& position : Positions){

        // Stop once empty position is reached //
        if(!position.WidgetToPosition)
            break;

        position.WidgetToPosition->Widget->Deselect();
    }
}

void SuperContainer::SelectAllItems(){

    for(auto& position : Positions){

        // Stop once empty position is reached //
        if(!position.WidgetToPosition)
            break;

        position.WidgetToPosition->Widget->Select();
    }
}
// ------------------------------------ //
void SuperContainer::Reflow(size_t index){

    if(Positions.empty() || index >= Positions.size())
        return;

    LayoutDirty = true;

    // The first one doesn't have a previous position //
    if(index == 0){

        LastWidthReflow = get_width();
        
        _PositionGridPosition(Positions.front(), nullptr, Positions.size());
        ++index;
    }

    // This is a check for debugging
    //_CheckPositions();
    
    for(size_t i = index; i < Positions.size(); ++i){

        LEVIATHAN_ASSERT(i > 0, "Positions reflow loop started too early");

        _PositionGridPosition(Positions[i], &Positions[i - 1], i - 1);
    }
}

void SuperContainer::_PositionGridPosition(GridPosition& current,
    const GridPosition* const previous, size_t previousindex) const
{
    // First is at fixed position //
    if(previous == nullptr){

        current.X = SUPERCONTAINER_MARGIN;
        current.Y = SUPERCONTAINER_MARGIN;
        return;
    }

    LEVIATHAN_ASSERT(previousindex < Positions.size(), "previousindex is out of range");

    // Check does it fit on the line //

    if(previous->X + previous->Width + SUPERCONTAINER_PADDING + current.Width <= get_width()){

        // It fits on the line //
        current.Y = previous->Y;
        current.X = previous->X + previous->Width + SUPERCONTAINER_PADDING;
        return;
    }

    // A new line is needed //

    // Find the tallest element in the last row
    int32_t lastRowMaxHeight = previous->Height;

    // Start from the one before previous, doesn't matter if the index wraps around as
    // the loop won't be entered in that case
    size_t scanIndex = previousindex - 1;

    const auto rowY = previous->Y;

    while(scanIndex < Positions.size()){

        if(Positions[scanIndex].Y != rowY){

            // Full row scanned //
            break;
        }

        // Check current height //
        const auto currentHeight = Positions[scanIndex].Height;

        if(currentHeight > lastRowMaxHeight)
            lastRowMaxHeight = currentHeight;

        // Move to previous //
        --scanIndex;
        if(scanIndex == 0)
            break;
    }

    // Position according to the maximum height of the last row
    current.X = SUPERCONTAINER_MARGIN;
    current.Y = previous->Y + lastRowMaxHeight + SUPERCONTAINER_PADDING;
}

SuperContainer::GridPosition& SuperContainer::_AddNewGridPosition(
    int32_t width, int32_t height)
{
    GridPosition pos;
    pos.Width = width;
    pos.Height = height;

    if(Positions.empty()){

        _PositionGridPosition(pos, nullptr, 0);

    } else {

        _PositionGridPosition(pos, &Positions.back(), Positions.size() - 1);
    }
    
    Positions.push_back(pos);

    return Positions.back();
}
// ------------------------------------ //
void SuperContainer::_SetWidgetSize(Element &widget){

    int width_min, width_natural;
    int height_min, height_natural;

    Container.add(*widget.Widget);
    widget.Widget->show();
    
    widget.Widget->get_preferred_width(width_min, width_natural);
    widget.Widget->get_preferred_height_for_width(width_natural, height_min, height_natural);

    widget.Width = width_natural;
    widget.Height = height_natural;
        
    widget.Widget->set_size_request(widget.Width, widget.Height);
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
    _SetWidgetSize(*widget);

    // Set it //
    if(Positions[index].SetNewWidget(widget)){

        // Do a reflow //
        Reflow(index);
        
    } else {

        // Apply positioning now //
        if(!LayoutDirty){
            
            _ApplyWidgetPosition(Positions[index]);
            UpdateRowWidths();
        }
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

void SuperContainer::_AddWidgetToEnd(std::shared_ptr<ResourceWithPreview> item,
    const ItemSelectable &selectable)
{
    // Create the widget //
    auto element = std::make_shared<Element>(item, selectable);

    // Initialize a size for the widget
    _SetWidgetSize(*element);

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
    GridPosition& pos = _AddNewGridPosition(element->Width, element->Height);

    pos.WidgetToPosition = element;

    if(!LayoutDirty){
        
        _ApplyWidgetPosition(pos);
        UpdateRowWidths();
    }
}
// ------------------------------------ //
void SuperContainer::_CheckPositions() const{

    // Find duplicate stuff //
    for(size_t i = 0; i < Positions.size(); ++i){

        for(size_t a = 0; a < Positions.size(); ++a){

            if(a == i)
                continue;

            if(Positions[i].WidgetToPosition.get() == Positions[a].WidgetToPosition.get()){

                LEVIATHAN_ASSERT(false,
                    "SuperContainer::_CheckPositions: duplicate Element ptr");
            }

            if(Positions[i].X == Positions[a].X &&
                Positions[i].Y == Positions[a].Y)
            {
                LEVIATHAN_ASSERT(false,
                    "SuperContainer::_CheckPositions: duplicate position");
            }

            if(Positions[i].WidgetToPosition->Widget.get() ==
                Positions[a].WidgetToPosition->Widget.get()){

                LEVIATHAN_ASSERT(false,
                    "SuperContainer::_CheckPositions: duplicate ListItem ptr");
            }
        }
    }
}
// ------------------------------------ //
// Callbacks
void SuperContainer::_OnResize(Gtk::Allocation &allocation){

    if(Positions.empty())
        return;
    
    // Skip if width didn't change //
    if(allocation.get_width() == LastWidthReflow)
        return;

    // Even if we don't reflow we don't want to be called again with the same width
    LastWidthReflow = allocation.get_width();
    bool reflow = false;

    // If doesn't fit dual margins
    if(allocation.get_width() < WidestRow + SUPERCONTAINER_MARGIN){

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

        // Forces update of positions
        Container.check_resize();
    }
}

// ------------------------------------ //
// GridPosition
bool SuperContainer::GridPosition::SetNewWidget(std::shared_ptr<Element> widget){

    const auto newWidth = widget->Width;
    const auto newHeight = widget->Height;

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
