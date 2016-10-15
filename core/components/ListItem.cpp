// ------------------------------------ //
#include "ListItem.h"


using namespace DV;
// ------------------------------------ //

ListItem::ListItem(std::shared_ptr<Image> showimage, const std::string &name,
    const ItemSelectable &selectable, bool allowpopup) :
    Container(Gtk::ORIENTATION_VERTICAL),
    ImageIcon(showimage, allowpopup ? SuperViewer::ENABLED_EVENTS::POPUP :
        SuperViewer::ENABLED_EVENTS::NONE, true),
    Selectable(selectable.Selectable),
    AllowPopUpWIndow(allowpopup),
    OnSelected(selectable.UpdateCallback)
{
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

    //NameLabel.set_selectable(true);
    NameLabel.set_margin_left(4);
    NameLabel.set_ellipsize(Pango::ELLIPSIZE_END);
    NameLabel.set_lines(4);
    NameLabel.set_line_wrap(true);
    NameLabel.set_line_wrap_mode(Pango::WRAP_WORD_CHAR);
    
    NameLabel.show();
    
    _SetName(name);

    //TextAreaOverlay.set_valign(Gtk::ALIGN_CENTER);

    // Click events //
    if(Selectable || allowpopup){

        LOG_INFO("Registered for events");
        Events.add_events(Gdk::BUTTON_PRESS_MASK);

        Events.signal_button_press_event().connect(
            sigc::mem_fun(*this, &ListItem::_OnMouseButtonPressed));
    }

    // TODO: allow editing name
}

ListItem::~ListItem(){

}
// ------------------------------------ //
void ListItem::_SetName(const std::string &name){

    NameLabel.set_text(name);
}

void ListItem::_SetImage(std::shared_ptr<Image> image){

    ImageIcon.SetImage(image);
}

// ------------------------------------ //
// Gtk overrides
Gtk::SizeRequestMode ListItem::get_request_mode_vfunc() const{

    if(ConstantSize)
        return Gtk::SIZE_REQUEST_CONSTANT_SIZE;
    
    //return Gtk::SIZE_REQUEST_CONSTANT_SIZE;
    return Gtk::SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

void ListItem::get_preferred_width_vfunc(int& minimum_width, int& natural_width) const{

    int box_width_min, box_width_natural;
    //Container.get_preferred_width(box_width_min, box_width_natural);
    Events.get_preferred_width(box_width_min, box_width_natural);
    
    if(ConstantSize){

        minimum_width = natural_width = 96;

    } else {
        minimum_width = 64;
        natural_width = 128;
    }
}

void ListItem::get_preferred_height_vfunc(int& minimum_height, int& natural_height) const{

    int box_height_min, box_height_natural;
    //Container.get_preferred_height(box_height_min, box_height_natural);
    Events.get_preferred_height(box_height_min, box_height_natural);

    if(ConstantSize){

        minimum_height = natural_height = 146;

    } else {

        minimum_height = 64;
        natural_height = 126;
    }
}

void ListItem::get_preferred_height_for_width_vfunc(int width,
    int& minimum_height, int& natural_height) const
{
    minimum_height = 64;

    natural_height = (int)((float)width / (3.f/4.f));
}
// ------------------------------------ //
bool ListItem::_OnMouseButtonPressed(GdkEventButton* event){

    if(!Selectable && !AllowPopUpWIndow)
        return false;

    if(event->type == GDK_2BUTTON_PRESS){

        if(AllowPopUpWIndow)
            _DoPopup();
        return true;
    }

    // Left mouse //
    if(event->button == 1){

        SetSelected(!CurrentlySelected);
    }

    return true;
}

void ListItem::SetSelected(bool selected){

    CurrentlySelected = selected;

    if(!CurrentlySelected){

        Container.unset_background_color();
        
    } else {
        
        // CadetBlue
        Container.override_background_color(Gdk::RGBA("CadetBlue"));
    }
    
    _OnSelectionUpdated();
}
// ------------------------------------ //
std::shared_ptr<Image> ListItem::GetPrimaryImage() const{

    return ImageIcon.GetImage();
}
// ------------------------------------ //
void ListItem::_OnSelectionUpdated(){

    if(OnSelected)
        OnSelected(*this);
}

void ListItem::_DoPopup(){

    LOG_INFO("Popup from listitem");
}
