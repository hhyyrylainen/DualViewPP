// ------------------------------------ //
#include "CollectionView.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //
CollectionView::CollectionView(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{

    signal_delete_event().connect(sigc::mem_fun(*this, &CollectionView::_OnClose));
    
    Gtk::Fixed* box;
    builder->get_widget("ImageContainer", box);
    LEVIATHAN_ASSERT(box, "Invalid .glade file");

    for(int i = 0; i < 25; ++i){

        auto thing = std::make_shared<ListItem>(
            Image::Create("/home/hhyyrylainen/690806.jpg"), "image " + Convert::ToString(i)
            + ".jpg");
        
        int width_min, width_natural;
        int height_min, height_natural;

        thing->show();
        
        thing->get_preferred_width(width_min, width_natural);
        thing->get_preferred_height_for_width(width_natural, height_min, height_natural);

        thing->set_size_request(width_natural, height_natural);
        
        box->put(*thing, (width_natural + 2) * i, 5);
        

        stuffs.push_back(thing);
    }
}

CollectionView::~CollectionView(){

    LOG_INFO("CollectionView closed");
}
// ------------------------------------ //
bool CollectionView::_OnClose(GdkEventAny* event){

    // Just hide it //
    hide();

    LOG_INFO("Hiding CollectionView");
    
    return true;
}



