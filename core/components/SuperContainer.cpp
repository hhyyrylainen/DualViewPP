// ------------------------------------ //
#include "SuperContainer.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //

SuperContainer::SuperContainer(_GtkScrolledWindow* widget, Glib::RefPtr<Gtk::Builder> builder)
    : Gtk::ScrolledWindow(widget), View(get_hadjustment(), get_vadjustment())
{
    add(View);
    View.add(Container);
    View.show();
    Container.show();
    
    // for(int i = 0; i < 25; ++i){

    //     auto thing = std::make_shared<ListItem>(
    //         Image::Create("/home/hhyyrylainen/690806.jpg"), "image " + Convert::ToString(i)
    //         + ".jpg");
        
    //     int width_min, width_natural;
    //     int height_min, height_natural;

    //     thing->show();
        
    //     thing->get_preferred_width(width_min, width_natural);
    //     thing->get_preferred_height_for_width(width_natural, height_min, height_natural);

    //     thing->set_size_request(width_natural, height_natural);
        
    //     Container.put(*thing, (width_natural + 2) * i, 5);
        

    //     stuffs.push_back(thing);
    // }
}

SuperContainer::~SuperContainer(){

}
// ------------------------------------ //


// ------------------------------------ //
void SuperContainer::_SetKeepFalse(){

    
}

void SuperContainer::_RemoveElementsNotMarkedKeep(){

    
}

void SuperContainer::_RemoveWidget(size_t index){

    LayoutDirty = true;
}
