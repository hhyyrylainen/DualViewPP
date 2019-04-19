// ------------------------------------ //
#include "SuperContainer.h"

#include "Common.h"
#include "DualView.h"

using namespace DV;
// ------------------------------------ //

//#define SUPERCONTAINER_RESIZE_REFLOW_CHECK_ONLY_FIRST_ROW

SuperContainer::SuperContainer() :
    Gtk::ScrolledWindow(), View(get_hadjustment(), get_vadjustment())
{
    _CommonCtor();
}

SuperContainer::SuperContainer(
    _GtkScrolledWindow* widget, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::ScrolledWindow(widget),
    View(get_hadjustment(), get_vadjustment())
{
    _CommonCtor();
}

void SuperContainer::_CommonCtor()
{
    add(View);
    View.add(Container);
    View.show();
    Container.show();
    PositionIndicator.property_width_request() = 2;
    PositionIndicator.get_style_context()->add_class("PositionIndicator");
    Container.add(PositionIndicator);
    PositionIndicator.hide();

    signal_size_allocate().connect(sigc::mem_fun(*this, &SuperContainer::_OnResize));

    signal_button_press_event().connect(
        sigc::mem_fun(*this, &SuperContainer::_OnMouseButtonPressed));

    // Both scrollbars need to be able to appear, otherwise the width cannot be reduced
    // so that wrapping occurs
    set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
}

SuperContainer::~SuperContainer()
{
    Clear();
}
// ------------------------------------ //
void SuperContainer::Clear(bool deselect /*= false*/)
{
    DualView::IsOnMainThreadAssert();

    // This could be made more efficient
    if(deselect)
        DeselectAllItems();

    // This will delete all the widgets //
    Positions.clear();
    _PositionIndicator();
    LayoutDirty = false;
}
// ------------------------------------ //
void SuperContainer::UpdatePositioning()
{
    if(!LayoutDirty)
        return;

    LayoutDirty = false;
    WidestRow = Margin;

    if(Positions.empty()) {
        _PositionIndicator();
        return;
    }

    int32_t CurrentRow = Margin;
    int32_t CurrentY = Positions.front().Y;

    for(auto& position : Positions) {

        if(position.Y != CurrentY) {

            // Row changed //
            if(WidestRow < CurrentRow)
                WidestRow = CurrentRow;

            CurrentRow = position.X;
            CurrentY = position.Y;
        }

        CurrentRow += position.Width + Padding;

        _ApplyWidgetPosition(position);
    }

    // Last row needs to be included, too //
    if(WidestRow < CurrentRow)
        WidestRow = CurrentRow;

    // Add margin
    WidestRow += Margin;

    _PositionIndicator();
}

void SuperContainer::UpdateRowWidths()
{
    WidestRow = Margin;

    int32_t CurrentRow = Margin;
    int32_t CurrentY = Positions.front().Y;

    for(auto& position : Positions) {

        if(position.Y != CurrentY) {

            // Row changed //
            if(WidestRow < CurrentRow)
                WidestRow = CurrentRow;

            CurrentRow = position.X;
            CurrentY = position.Y;
        }

        CurrentRow += position.Width + Padding;
    }

    // Last row needs to be included, too //
    if(WidestRow < CurrentRow)
        WidestRow = CurrentRow;

    // Add margin
    WidestRow += Margin;
}
// ------------------------------------ //
size_t SuperContainer::CountRows() const
{
    size_t count = 0;

    int32_t CurrentY = -1;

    for(auto& position : Positions) {

        // Stop once empty position is reached //
        if(!position.WidgetToPosition)
            break;

        if(position.Y != CurrentY) {

            ++count;
            CurrentY = position.Y;
        }
    }

    return count;
}

size_t SuperContainer::CountItems() const
{
    size_t count = 0;

    for(auto& position : Positions) {

        // Stop once empty position is reached //
        if(!position.WidgetToPosition)
            break;

        ++count;
    }

    return count;
}
// ------------------------------------ //
size_t SuperContainer::CountSelectedItems() const
{
    size_t count = 0;

    for(auto& position : Positions) {

        // Stop once empty position is reached //
        if(!position.WidgetToPosition)
            break;

        if(position.WidgetToPosition->Widget->IsSelected())
            ++count;
    }

    return count;
}

void SuperContainer::DeselectAllItems()
{
    for(auto& position : Positions) {

        // Stop once empty position is reached //
        if(!position.WidgetToPosition)
            break;

        position.WidgetToPosition->Widget->Deselect();
    }
}

void SuperContainer::SelectAllItems()
{
    for(auto& position : Positions) {

        // Stop once empty position is reached //
        if(!position.WidgetToPosition)
            break;

        position.WidgetToPosition->Widget->Select();
    }
}

void SuperContainer::DeselectAllExcept(const ListItem* item)
{
    for(auto& position : Positions) {

        // Stop once empty position is reached //
        if(!position.WidgetToPosition)
            break;

        if(position.WidgetToPosition->Widget.get() == item)
            continue;

        position.WidgetToPosition->Widget->Deselect();
    }
}

void SuperContainer::DeselectFirstItem()
{
    for(auto& position : Positions) {

        // Stop once empty position is reached //
        if(!position.WidgetToPosition)
            break;

        if(position.WidgetToPosition->Widget->IsSelected()) {

            position.WidgetToPosition->Widget->Deselect();
            return;
        }
    }
}

void SuperContainer::SelectFirstItems(int count)
{
    int selected = 0;

    for(auto& position : Positions) {

        // If enough are selected stop
        if(selected >= count)
            break;

        // Stop once empty position is reached //
        if(!position.WidgetToPosition)
            break;

        position.WidgetToPosition->Widget->Select();
        ++selected;
    }
}

void SuperContainer::SelectNextItem()
{
    bool select = false;

    for(auto iter = Positions.begin(); iter != Positions.end(); ++iter) {

        auto& position = *iter;

        // Stop once empty position is reached //
        if(!position.WidgetToPosition)
            break;

        if(select == true) {

            position.WidgetToPosition->Widget->Select();
            DeselectAllExcept(position.WidgetToPosition->Widget.get());
            break;
        }

        if(position.WidgetToPosition->Widget->IsSelected()) {

            select = true;
        }
    }

    if(!select) {

        // None selected //
        SelectFirstItem();
    }
}

void SuperContainer::SelectPreviousItem()
{
    bool select = false;

    for(auto iter = Positions.rbegin(); iter != Positions.rend(); ++iter) {

        auto& position = *iter;

        // When reversing we can't stop when the tailing empty slots are reached
        if(!position.WidgetToPosition)
            continue;

        if(select == true) {

            position.WidgetToPosition->Widget->Select();
            DeselectAllExcept(position.WidgetToPosition->Widget.get());
            break;
        }

        if(position.WidgetToPosition->Widget->IsSelected()) {

            select = true;
        }
    }

    if(!select) {

        // None selected //
        SelectFirstItem();
    }
}
// ------------------------------------ //
bool SuperContainer::IsEmpty() const
{
    for(auto& position : Positions) {

        // Stop once empty position is reached //
        if(!position.WidgetToPosition)
            break;

        // Found non-empty
        return false;
    }

    return true;
}
// ------------------------------------ //
std::shared_ptr<ResourceWithPreview> SuperContainer::GetFirstVisibleResource(
    double scrollOffset) const
{
    for(const auto& position : Positions) {

        if(((position.Y + 5) > scrollOffset) && position.WidgetToPosition) {

            return position.WidgetToPosition->CreatedFrom;
        }
    }

    return nullptr;
}

std::vector<std::shared_ptr<ResourceWithPreview>> SuperContainer::GetResourcesVisibleAfter(
    double scrollOffset) const
{
    std::vector<std::shared_ptr<ResourceWithPreview>> result;
    result.reserve(Positions.size() / 3);

    for(const auto& position : Positions) {

        if(((position.Y + 5) > scrollOffset) && position.WidgetToPosition) {

            result.push_back(position.WidgetToPosition->CreatedFrom);
        }
    }

    return result;
}

double SuperContainer::GetResourceOffset(std::shared_ptr<ResourceWithPreview> resource) const
{
    for(const auto& position : Positions) {

        if(position.WidgetToPosition && position.WidgetToPosition->CreatedFrom == resource) {

            return position.Y;
        }
    }

    return -1;
}
// ------------------------------------ //
void SuperContainer::UpdateMarginAndPadding(int newmargin, int newpadding)
{
    Margin = newmargin;
    Padding = newpadding;

    LayoutDirty = true;
    Reflow(0);
    UpdatePositioning();
}
// ------------------------------------ //
void SuperContainer::Reflow(size_t index)
{
    if(Positions.empty() || index >= Positions.size())
        return;

    LayoutDirty = true;

    // The first one doesn't have a previous position //
    if(index == 0) {

        LastWidthReflow = get_width();

        _PositionGridPosition(Positions.front(), nullptr, Positions.size());
        ++index;
    }

    // This is a check for debugging
    //_CheckPositions();

    for(size_t i = index; i < Positions.size(); ++i) {

        _PositionGridPosition(Positions[i], &Positions[i - 1], i - 1);
    }
}

void SuperContainer::_PositionGridPosition(
    GridPosition& current, const GridPosition* const previous, size_t previousindex) const
{
    // First is at fixed position //
    if(previous == nullptr) {

        current.X = Margin;
        current.Y = Margin;
        return;
    }

    LEVIATHAN_ASSERT(previousindex < Positions.size(), "previousindex is out of range");

    // Check does it fit on the line //

    if(previous->X + previous->Width + Padding + current.Width <= get_width()) {

        // It fits on the line //
        current.Y = previous->Y;
        current.X = previous->X + previous->Width + Padding;
        return;
    }

    // A new line is needed //

    // Find the tallest element in the last row
    int32_t lastRowMaxHeight = previous->Height;

    // Start from the one before previous, doesn't matter if the index wraps around as
    // the loop won't be entered in that case
    size_t scanIndex = previousindex - 1;

    const auto rowY = previous->Y;

    while(scanIndex < Positions.size()) {

        if(Positions[scanIndex].Y != rowY) {

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
    current.X = Margin;
    current.Y = previous->Y + lastRowMaxHeight + Padding;
}

SuperContainer::GridPosition& SuperContainer::_AddNewGridPosition(
    int32_t width, int32_t height)
{
    GridPosition pos;
    pos.Width = width;
    pos.Height = height;

    if(Positions.empty()) {

        _PositionGridPosition(pos, nullptr, 0);

    } else {

        _PositionGridPosition(pos, &Positions.back(), Positions.size() - 1);
    }

    Positions.push_back(pos);

    return Positions.back();
}
// ------------------------------------ //
void SuperContainer::_SetWidgetSize(Element& widget)
{
    int width_min, width_natural;
    int height_min, height_natural;

    widget.Widget->SetItemSize(SelectedItemSize);

    Container.add(*widget.Widget);
    widget.Widget->show();

    widget.Widget->get_preferred_width(width_min, width_natural);
    widget.Widget->get_preferred_height_for_width(width_natural, height_min, height_natural);

    widget.Width = width_natural;
    widget.Height = height_natural;

    widget.Widget->set_size_request(widget.Width, widget.Height);
}

void SuperContainer::SetItemSize(LIST_ITEM_SIZE newsize)
{
    if(SelectedItemSize == newsize)
        return;

    SelectedItemSize = newsize;

    if(Positions.empty())
        return;

    // Resize all elements //
    for(const auto& position : Positions) {

        if(position.WidgetToPosition) {

            _SetWidgetSize(*position.WidgetToPosition);
        }
    }

    LayoutDirty = true;
    UpdatePositioning();
}
// ------------------------------------ //
void SuperContainer::_SetKeepFalse()
{
    for(auto& position : Positions) {

        if(position.WidgetToPosition)
            position.WidgetToPosition->Keep = false;
    }
}

void SuperContainer::_RemoveElementsNotMarkedKeep()
{
    size_t reflowStart = Positions.size();

    for(size_t i = 0; i < Positions.size();) {

        auto& current = Positions[i];

        // If the current position has no widget try to get the next widget, or end //
        if(!current.WidgetToPosition) {

            if(i + 1 < Positions.size()) {

                // Swap with the next one //
                if(Positions[i].SwapWidgets(Positions[i + 1])) {

                    if(reflowStart > i)
                        reflowStart = i;
                }
            }

            // If still empty there are no more widgets to process
            // It doesn't seem to be right for some reason to break here
            if(!current.WidgetToPosition) {

                ++i;
                continue;
            }
        }

        if(!Positions[i].WidgetToPosition->Keep) {

            LayoutDirty = true;

            // Remove this widget //
            Container.remove(*current.WidgetToPosition->Widget);
            current.WidgetToPosition.reset();

            // On the next iteration the next widget will be moved to this position

        } else {

            ++i;
        }
    }

    if(reflowStart < Positions.size()) {

        // Need to reflow //
        Reflow(reflowStart);
    }
}
// ------------------------------------ //
void SuperContainer::_RemoveWidget(size_t index)
{
    if(index >= Positions.size())
        throw Leviathan::InvalidArgument("index out of range");

    LayoutDirty = true;

    size_t reflowStart = Positions.size();

    Container.remove(*Positions[index].WidgetToPosition->Widget);
    Positions[index].WidgetToPosition.reset();

    // Move forward all the widgets //
    for(size_t i = index; i + 1 < Positions.size(); ++i) {

        if(Positions[i].SwapWidgets(Positions[i + 1])) {

            if(reflowStart > i)
                reflowStart = i;
        }
    }

    if(reflowStart < Positions.size()) {

        // Need to reflow //
        Reflow(reflowStart);
    }
}

void SuperContainer::_SetWidget(
    size_t index, std::shared_ptr<Element> widget, bool autoreplace)
{
    if(index >= Positions.size())
        throw Leviathan::InvalidArgument("index out of range");

    if(Positions[index].WidgetToPosition) {

        if(!autoreplace) {

            throw Leviathan::InvalidState("index is not empty and no autoreplace specified");

        } else {

            // Remove the current one
            Container.remove(*Positions[index].WidgetToPosition->Widget);
        }
    }

    // Initialize a size for the widget
    _SetWidgetSize(*widget);

    // Set it //
    if(Positions[index].SetNewWidget(widget)) {

        // Do a reflow //
        Reflow(index);

    } else {

        // Apply positioning now //
        if(!LayoutDirty) {

            _ApplyWidgetPosition(Positions[index]);
            UpdateRowWidths();
            _PositionIndicator();
        }
    }
}
// ------------------------------------ //
void SuperContainer::_PushBackWidgets(size_t index)
{
    if(Positions.empty())
        return;

    LayoutDirty = true;

    size_t reflowStart = Positions.size();

    // Create a new position and then pull back widgets until index is reached and then
    // stop

    // We can skip adding if the last is empty //
    if(Positions.back().WidgetToPosition)
        _AddNewGridPosition(Positions.back().Width, Positions.back().Height);

    for(size_t i = Positions.size() - 1; i > index; --i) {

        // Swap pointers around, the current index is always empty so the empty
        // spot will propagate to index
        // Also i - 1 cannot be out of range as the smallest index 0 wouldn't enter
        // this loop

        if(Positions[i].SwapWidgets(Positions[i - 1])) {

            if(reflowStart > i)
                reflowStart = i;
        }
    }

    if(reflowStart < Positions.size()) {

        // Need to reflow //
        Reflow(reflowStart);
    }
}

void SuperContainer::_AddWidgetToEnd(std::shared_ptr<ResourceWithPreview> item,
    const std::shared_ptr<ItemSelectable>& selectable)
{
    // Create the widget //
    auto element = std::make_shared<Element>(item, selectable);

    // Initialize a size for the widget
    _SetWidgetSize(*element);

    // Find the first empty spot //
    for(size_t i = 0; i < Positions.size(); ++i) {

        if(!Positions[i].WidgetToPosition) {

            if(Positions[i].SetNewWidget(element)) {

                // Do a reflow //
                Reflow(i);
            }

            return;
        }
    }

    // No empty spots, create a new one //
    GridPosition& pos = _AddNewGridPosition(element->Width, element->Height);

    pos.WidgetToPosition = element;

    if(!LayoutDirty) {

        _ApplyWidgetPosition(pos);
        UpdateRowWidths();
        _PositionIndicator();
    }
}
// ------------------------------------ //
void SuperContainer::_CheckPositions() const
{
    // Find duplicate stuff //
    for(size_t i = 0; i < Positions.size(); ++i) {

        for(size_t a = 0; a < Positions.size(); ++a) {

            if(a == i)
                continue;

            if(Positions[i].WidgetToPosition.get() == Positions[a].WidgetToPosition.get()) {

                LEVIATHAN_ASSERT(
                    false, "SuperContainer::_CheckPositions: duplicate Element ptr");
            }

            if(Positions[i].X == Positions[a].X && Positions[i].Y == Positions[a].Y) {
                LEVIATHAN_ASSERT(false, "SuperContainer::_CheckPositions: duplicate position");
            }

            if(Positions[i].WidgetToPosition->Widget.get() ==
                Positions[a].WidgetToPosition->Widget.get()) {

                LEVIATHAN_ASSERT(
                    false, "SuperContainer::_CheckPositions: duplicate ListItem ptr");
            }
        }
    }
}
// ------------------------------------ //
// Position indicator
void SuperContainer::EnablePositionIndicator()
{
    if(PositionIndicatorEnabled)
        return;
    PositionIndicatorEnabled = true;

    // Enable the click to change indicator position
    add_events(Gdk::BUTTON_PRESS_MASK);

    _PositionIndicator();
}

size_t SuperContainer::GetIndicatorPosition() const
{
    return IndicatorPosition;
}

void SuperContainer::SetIndicatorPosition(size_t position)
{
    if(IndicatorPosition == position)
        return;

    IndicatorPosition = position;
    _PositionIndicator();
}

void SuperContainer::SuperContainer::_PositionIndicator()
{
    if(!PositionIndicatorEnabled) {
        return;
    }

    constexpr auto indicatorHeightSmallerBy = 6;

    // Detect emptyness, but also the widget size
    bool found = false;

    for(const auto& position : Positions) {

        if(position.WidgetToPosition) {

            found = true;
            PositionIndicator.property_height_request() =
                position.WidgetToPosition->Height - indicatorHeightSmallerBy;
            break;
        }
    }

    if(!found) {
        // Empty
        PositionIndicator.property_visible() = false;
        return;
    }

    PositionIndicator.property_visible() = true;

    // Optimization for last
    if(IndicatorPosition >= Positions.size()) {

        for(size_t i = Positions.size() - 1;; --i) {

            const auto& position = Positions[i];

            if(position.WidgetToPosition) {

                // Found the end
                Container.move(PositionIndicator, position.X + position.Width + Padding / 2,
                    position.Y + indicatorHeightSmallerBy / 2);
                return;
            }

            if(i == 0) {
                break;
            }
        }
    } else {
        if(IndicatorPosition == 0) {
            // Optimization for first
            Container.move(
                PositionIndicator, Margin / 2, Margin + indicatorHeightSmallerBy / 2);
            return;

        } else {
            // Find a suitable position (or leave hidden)
            bool after = false;

            for(size_t i = IndicatorPosition;; --i) {

                const auto& position = Positions[i];

                if(position.WidgetToPosition) {

                    Container.move(PositionIndicator,
                        after ? position.X + Positions[i].Width + Padding / 2 :
                                position.X - Padding / 2,
                        position.Y + indicatorHeightSmallerBy / 2);
                    return;

                } else {
                    after = true;
                }

                if(i == 0) {
                    break;
                }
            }
        }
    }

    LOG_ERROR("SuperContainer: failed to find position for indicator");
    PositionIndicator.property_visible() = false;
}
// ------------------------------------ //
// Callbacks
void SuperContainer::_OnResize(Gtk::Allocation& allocation)
{
    if(Positions.empty())
        return;

    // Skip if width didn't change //
    if(allocation.get_width() == LastWidthReflow)
        return;

    // Even if we don't reflow we don't want to be called again with the same width
    LastWidthReflow = allocation.get_width();
    bool reflow = false;

    // If doesn't fit dual margins
    if(allocation.get_width() < WidestRow + Margin) {

        // Rows don't fit anymore //
        reflow = true;

    } else {

        // Check would wider rows fit //
        int32_t CurrentRow = 0;
        int32_t CurrentY = Positions.front().Y;

        for(auto& position : Positions) {

            if(position.Y != CurrentY) {

                // Row changed //

                if(Margin + CurrentRow + Padding + position.Width < allocation.get_width()) {
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

    if(reflow) {

        Reflow(0);
        UpdatePositioning();

        // Forces update of positions
        Container.check_resize();
    }
}

bool SuperContainer::_OnMouseButtonPressed(GdkEventButton* event)
{
    if(event->type == GDK_BUTTON_PRESS) {

        // Determine where to place the indicator
        size_t newPosition = -1;

        for(size_t i = 0; i < Positions.size(); ++i) {

            const auto& position = Positions[i];

            if(!position.WidgetToPosition)
                break;

            // If click is to the left of position this is the target
            if(event->y >= position.Y - Padding &&
                event->y < position.Y + position.Height + Padding && position.X >= event->x) {
                newPosition = i;
                break;
            }
        }

        SetIndicatorPosition(newPosition);
        return true;
    }

    return false;
}

// ------------------------------------ //
// GridPosition
bool SuperContainer::GridPosition::SetNewWidget(std::shared_ptr<Element> widget)
{
    const auto newWidth = widget->Width;
    const auto newHeight = widget->Height;

    WidgetToPosition = widget;

    if(newWidth != Width && newHeight != Height) {

        Width = newWidth;
        Height = newHeight;
        return true;
    }

    return false;
}

bool SuperContainer::GridPosition::SwapWidgets(GridPosition& other)
{
    WidgetToPosition.swap(other.WidgetToPosition);

    if(Width != other.Width || Height != other.Height) {

        Width = other.Width;
        Height = other.Height;
        return true;
    }

    return false;
}

std::string SuperContainer::GridPosition::ToString() const
{
    return "[" + std::to_string(X) + ", " + std::to_string(Y) +
           " dim: " + std::to_string(Width) + ", " + std::to_string(Height) + " " +
           (WidgetToPosition ? "(filled)" : "(empty)");
}
