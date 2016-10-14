// ------------------------------------ //
#include "Importer.h"

#include "core/components/SuperViewer.h"
#include "core/components/SuperContainer.h"
#include "Common.h"

using namespace DV;
// ------------------------------------ //

Importer::Importer(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    builder->get_widget_derived("PreviewImage", PreviewImage, nullptr);
    LEVIATHAN_ASSERT(PreviewImage, "Invalid .glade file");

    builder->get_widget_derived("ImageList", ImageList);
    LEVIATHAN_ASSERT(ImageList, "Invalid .glade file");
    
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

