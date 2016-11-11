// ------------------------------------ //
#include "DLListItem.h"

using namespace DV;
// ------------------------------------ //

DLListItem::DLListItem(){

    
    add(Container);
    
    Container.pack_start(Enabled, false, false);
    Enabled.set_state(true);
    Enabled.set_valign(Gtk::ALIGN_CENTER);

    show_all_children();
}

DLListItem::~DLListItem(){


}
