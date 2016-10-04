// ------------------------------------ //
#include "ListItem.h"


using namespace DV;
// ------------------------------------ //

ListItem::ListItem(std::shared_ptr<Image> showImage, const std::string &name) :
    Container(Gtk::ORIENTATION_VERTICAL),
    ImageIcon(showImage)
{
    add(Container);

    Container.set_homogeneous(false);
    Container.set_spacing(2);
    Container.show();


    Container.pack_start(ImageIcon, Gtk::PACK_EXPAND_WIDGET);
    ImageIcon.show();

    Container.pack_end(TextAreaOverlay, Gtk::PACK_SHRINK);
    TextAreaOverlay.add(NameLabel);

    TextAreaOverlay.set_margin_bottom(3);
    TextAreaOverlay.show();
    NameLabel.show();
    
    _SetName(name);

    TextAreaOverlay.set_valign(Gtk::ALIGN_CENTER);
    
    NameLabel.set_valign(Gtk::ALIGN_CENTER);
    NameLabel.set_halign(Gtk::ALIGN_FILL);
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
    Container.get_preferred_width(box_width_min, box_width_natural);
    
    if(ConstantSize){

        minimum_width = natural_width = 96;

    } else {
        minimum_width = 128;
        natural_width = 256;
    }
}

void ListItem::get_preferred_height_vfunc(int& minimum_height, int& natural_height) const{

    int box_height_min, box_height_natural;
    Container.get_preferred_height(box_height_min, box_height_natural);

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

