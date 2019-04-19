// ------------------------------------ //
#include "ListItem.h"

#include "ItemDragInformationProvider.h"

using namespace DV;
// ------------------------------------ //

ListItem::ListItem(std::shared_ptr<Image> showimage, const std::string& name,
    const std::shared_ptr<ItemSelectable>& selectable, bool allowpopup) :
    Container(Gtk::ORIENTATION_VERTICAL),
    ImageIcon(showimage,
        allowpopup ? SuperViewer::ENABLED_EVENTS::POPUP : SuperViewer::ENABLED_EVENTS::NONE,
        true),
    Selectable(selectable)
{
    // This above child property is needed for some reason to get mouse up events
    Events.property_above_child() = true;
    add(Events);
    Events.add(Container);
    Events.show();

    Container.set_homogeneous(false);
    Container.set_spacing(2);
    Container.show();


    Container.pack_start(ImageIcon, Gtk::PACK_EXPAND_WIDGET);
    ImageIcon.show();

    Container.pack_end(TextAreaOverlay, Gtk::PACK_SHRINK);
    TextAreaOverlay.add(NameLabel);

    TextAreaOverlay.set_margin_bottom(3);
    TextAreaOverlay.show();

    NameLabel.set_valign(Gtk::ALIGN_CENTER);
    NameLabel.set_halign(Gtk::ALIGN_FILL);

    // NameLabel.set_selectable(true);
    NameLabel.property_margin_start() = 4;
    NameLabel.set_ellipsize(Pango::ELLIPSIZE_END);
    NameLabel.set_lines(4);
    NameLabel.set_line_wrap(true);
    NameLabel.set_line_wrap_mode(Pango::WRAP_WORD_CHAR);

    NameLabel.show();

    _SetName(name);

    Container.get_style_context()->add_class("ListItemContainer");

    // TextAreaOverlay.set_valign(Gtk::ALIGN_CENTER);

    // Click events //
    if(Selectable && (Selectable->Selectable || Selectable->UsesCustomPopup ||
                         Selectable->DragInformation)) {

        Events.add_events(Gdk::BUTTON_PRESS_MASK);

        Events.signal_button_press_event().connect(
            sigc::mem_fun(*this, &ListItem::_OnMouseButtonPressed));

        if(Selectable->DragInformation) {

            Events.add_events(Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_RELEASE_MASK);

            Events.signal_button_release_event().connect(
                sigc::mem_fun(*this, &ListItem::_OnMouseButtonReleased));

            Events.signal_motion_notify_event().connect(
                sigc::mem_fun(*this, &ListItem::_OnMouseMove));

            signal_drag_data_get().connect(sigc::mem_fun(*this, &ListItem::_OnDragDataGet));
            signal_drag_end().connect(sigc::mem_fun(*this, &ListItem::_OnDragFinished));
        }
    }

    // TODO: allow editing name
}

ListItem::~ListItem() {}
// ------------------------------------ //
void ListItem::_SetName(const std::string& name)
{
    NameLabel.set_text(name);
}

void ListItem::_SetImage(std::shared_ptr<Image> image)
{
    ImageIcon.SetImage(image);
}

// ------------------------------------ //
// Gtk overrides
Gtk::SizeRequestMode ListItem::get_request_mode_vfunc() const
{
    return Gtk::SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

void ListItem::get_preferred_width_vfunc(int& minimum_width, int& natural_width) const
{
    int box_width_min, box_width_natural;
    // Container.get_preferred_width(box_width_min, box_width_natural);
    Events.get_preferred_width(box_width_min, box_width_natural);

    switch(ItemSize) {
    case LIST_ITEM_SIZE::NORMAL: {
        minimum_width = 64;
        natural_width = 128;
        break;
    }
    case LIST_ITEM_SIZE::SMALL: {
        minimum_width = 64;
        natural_width = 82;
        break;
    }
    case LIST_ITEM_SIZE::TINY: {
        minimum_width = 38;
        natural_width = 56;
        break;
    }
    }
}

void ListItem::get_preferred_height_vfunc(int& minimum_height, int& natural_height) const
{
    int box_height_min, box_height_natural;
    // Container.get_preferred_height(box_height_min, box_height_natural);
    Events.get_preferred_height(box_height_min, box_height_natural);

    switch(ItemSize) {
    case LIST_ITEM_SIZE::NORMAL: {
        minimum_height = 64;
        natural_height = 126;
        break;
    }
    case LIST_ITEM_SIZE::SMALL: {
        minimum_height = 64;
        natural_height = 92;
        break;
    }
    case LIST_ITEM_SIZE::TINY: {
        minimum_height = 38;
        natural_height = 60;
        break;
    }
    }
}

void ListItem::get_preferred_height_for_width_vfunc(
    int width, int& minimum_height, int& natural_height) const
{
    minimum_height = 64;

    natural_height = (int)((float)width / (3.f / 4.f));
}
// ------------------------------------ //
bool ListItem::_OnMouseButtonPressed(GdkEventButton* event)
{
    // Double click
    if(event->type == GDK_2BUTTON_PRESS && event->button == 1) {

        if(Selectable->UsesCustomPopup) {

            _DoPopup();
            return true;
        }

        return false;
    }

    // Left mouse //
    if(event->button == 1) {

        if(Active) {
            MouseDownPos = Point(event->x, event->y);
            MouseDown = true;
        }

        if(Active && Selectable && Selectable->Selectable) {

            SetSelected(!CurrentlySelected);
            return true;
        }

        return true;

    } else if(event->button == 3) {

        return _OnRightClick(event);
    }

    return false;
}

bool ListItem::_OnMouseButtonReleased(GdkEventButton* event)
{
    // Left mouse //
    if(event->button == 1) {

        DoingDrag = false;
        MouseDown = false;
        return true;
    }

    return false;
}

bool ListItem::_OnMouseMove(GdkEventMotion* motion_event)
{
    if(!(motion_event->state & Gdk::BUTTON_MOTION_MASK))
        return false;

    if(!MouseDown || !Active)
        return false;

    if(!DoingDrag) {
        float moved = (MouseDownPos - Point(motion_event->x, motion_event->y)).HAddAbs();

        if(moved > StartDragAfter && Selectable && Selectable->DragInformation) {

            LOG_INFO("Starting drag from ListItem");

            drag_begin(Gtk::TargetList::create(Selectable->DragInformation->GetDragTypes()),
                Gdk::DragAction::ACTION_COPY, motion_event->state,
                reinterpret_cast<GdkEvent*>(motion_event), motion_event->x, motion_event->y);
            DoingDrag = true;
            MouseDown = false;

            if(Active && Selectable && Selectable->Selectable) {

                SetSelected(!CurrentlySelected);
                return true;
            }

            return true;
        }
    }

    return false;
}

void ListItem::_OnDragDataGet(const Glib::RefPtr<Gdk::DragContext>& context,
    Gtk::SelectionData& selection_data, guint info, guint time)
{
    selection_data.set(selection_data.get_target(), 8 /* 8 bits format */,
        (const guchar*)"I'm Data!", 9 /* the length of I'm Data! in bytes */);
}

void ListItem::_OnDragFinished(const Glib::RefPtr<Gdk::DragContext>& context)
{
    DoingDrag = false;
}

// ------------------------------------ //
void ListItem::SetSelected(bool selected)
{
    if(CurrentlySelected == selected)
        return;

    CurrentlySelected = selected;

    if(!CurrentlySelected) {

        Container.get_style_context()->remove_class("ListItemContainerSelected");

    } else {

        Container.get_style_context()->add_class("ListItemContainerSelected");
    }

    _OnSelectionUpdated();
}
// ------------------------------------ //
void ListItem::SetActive(bool active)
{
    if(active == Active)
        return;

    Active = active;

    if(!Active && CurrentlySelected) {
        // Deselect before becoming inactive
        Deselect();
    }

    if(Active) {

        Container.get_style_context()->remove_class("ListItemContainerInactive");

    } else {

        Container.get_style_context()->add_class("ListItemContainerInactive");
    }
}

// ------------------------------------ //
void ListItem::SetItemSize(LIST_ITEM_SIZE newsize)
{
    ItemSize = newsize;

    switch(ItemSize) {
    case LIST_ITEM_SIZE::TINY: NameLabel.set_lines(1); break;
    default: NameLabel.set_lines(4);
    }
}

std::shared_ptr<Image> ListItem::GetPrimaryImage() const
{
    return ImageIcon.GetImage();
}
// ------------------------------------ //
void ListItem::_OnSelectionUpdated()
{
    if(Selectable->Selectable)
        Selectable->UpdateCallback(*this);
}

void ListItem::_DoPopup() {}

bool ListItem::_OnRightClick(GdkEventButton* causedbyevent)
{
    return false;
}

void ListItem::_OnInactiveStatusUpdated() {}
