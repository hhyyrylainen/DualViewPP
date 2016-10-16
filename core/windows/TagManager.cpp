// ------------------------------------ //
#include "TagManager.h"

using namespace DV;
// ------------------------------------ //
TagManager::TagManager(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &TagManager::_OnClose));

    signal_unmap().connect(sigc::mem_fun(*this, &TagManager::_OnHidden));
    signal_map().connect(sigc::mem_fun(*this, &TagManager::_OnShown));
    
    //builder->get_widget_derived("ImageContainer", Container);
    //LEVIATHAN_ASSERT(Container, "Invalid .glade file");
}

TagManager::~TagManager(){

}
// ------------------------------------ //
bool TagManager::_OnClose(GdkEventAny* event){

    // Just hide it //
    hide();

    return true;
}
// ------------------------------------ //
void TagManager::_OnShown(){

    // Load items //

}

void TagManager::_OnHidden(){

    // Explicitly unload items //
    
}

// ------------------------------------ //
void TagManager::SetCreateTag(const std::string){
    
        
}

