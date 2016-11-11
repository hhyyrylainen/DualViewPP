// ------------------------------------ //
#include "Downloader.h"

#include "core/components/DLListItem.h"

#include "core/DualView.h"
#include "Common.h"

using namespace DV;
// ------------------------------------ //
Downloader::Downloader(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &Downloader::_OnClose));

    signal_unmap().connect(sigc::mem_fun(*this, &Downloader::_OnHidden));
    signal_map().connect(sigc::mem_fun(*this, &Downloader::_OnShown));

    builder->get_widget("DLList", DLWidgets);
    LEVIATHAN_ASSERT(DLWidgets, "Invalid .glade file");

    
}

Downloader::~Downloader(){

}

// ------------------------------------ //
bool Downloader::_OnClose(GdkEventAny* event){

    // Just hide it //
    hide();

    return true;
}

void Downloader::_OnShown(){
    
    // Load items if not already loaded //
    auto item = std::make_shared<DLListItem>();
    DLList.push_back(item);

    DLWidgets->add(*item);
    item->show();
}

void Downloader::_OnHidden(){

    // Ask user whether downloads should be paused //
    
}
// ------------------------------------ //
