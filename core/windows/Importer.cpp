// ------------------------------------ //
#include "Importer.h"

using namespace DV;
// ------------------------------------ //

Importer::Importer(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder){

    signal_delete_event().connect(sigc::mem_fun(*this, &Importer::_OnClosed));
}

Importer::~Importer(){

    Close();
}
// ------------------------------------ //    
bool Importer::_OnClosed(GdkEventAny* event){

    _ReportClosed();
    return false;
}

void Importer::_OnClose(){

    close();

    // Todo: release logic
}

