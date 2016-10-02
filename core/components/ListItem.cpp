// ------------------------------------ //
#include "ListItem.h"


using namespace DV;
// ------------------------------------ //

ListItem::ListItem(std::shared_ptr<Image> showImage, const std::string &name) :
    Gtk::Box(Gtk::ORIENTATION_VERTICAL),
    ImageIcon(showImage)
{
    pack_start(ImageIcon);
    ImageIcon.show();

    pack_end(TextAreaOverlay);
    TextAreaOverlay.add(NameLabel);
    
    TextAreaOverlay.show();
    NameLabel.show();
    
    _SetName(name);
}

ListItem::~ListItem(){

}
// ------------------------------------ //
void ListItem::_SetName(const std::string &name){

    NameLabel.set_text(name);
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

    if(ConstantSize){

        minimum_width = natural_width = 96;

    } else {
        minimum_width = 128;
        natural_width = 256;
    }
}

void ListItem::get_preferred_height_vfunc(int& minimum_height, int& natural_height) const{

    if(ConstantSize){

        minimum_height = natural_height = 146;

    } else {

        minimum_height = 64;
        natural_height = 192;
    }
}

void ListItem::get_preferred_height_for_width_vfunc(int width,
    int& minimum_height, int& natural_height) const
{
    minimum_height = 64;

    natural_height = (int)((float)width / (3.f/4.f));
}

