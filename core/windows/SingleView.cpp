// ------------------------------------ //
#include "SingleView.h"

#include "core/resources/Image.h"

#include "Exceptions.h"
#include "DualView.h"
#include "Common.h"

using namespace DV;
// ------------------------------------ //

SingleView::SingleView(const std::string &file){

    // Create resource //
    auto resource = std::make_shared<Image>(file);


    // Load gtk stuff //

    Builder = Gtk::Builder::create_from_file(
        "../gui/single_view.glade");

    // Get all the glade resources //
    Builder->get_widget("SingleView", OurWindow);
    LEVIATHAN_ASSERT(OurWindow, "Invalid SingleView .glade file");

    OurWindow->signal_delete_event().connect(sigc::mem_fun(*this, &SingleView::_OnClosed));

    try{
        
        Builder->get_widget_derived("ImageView", ImageView, resource);
        
    } catch(const Leviathan::InvalidArgument &e){

        LOG_WARNING("SingleView: failed to create SuperViewer, exception: ");
        e.PrintToLog();
        throw;
    }
    
    LEVIATHAN_ASSERT(ImageView, "Invalid SingleView .glade file");
    
    DualView::Get().RegisterWindow(*OurWindow);
    OurWindow->show();
}

SingleView::~SingleView(){

    Close();
}
// ------------------------------------ //
void SingleView::Close(){

    // Do nothing if already closed //
    if(!OurWindow)
        return;

    LOG_INFO("SingleView window closed");

    OurWindow->close();
    OurWindow = nullptr;

    // Destroy the widgets from the Builder //
    Builder.reset();
}
// ------------------------------------ //
bool SingleView::_OnClosed(GdkEventAny* event){

    _ReportClosed();
    
    // Allow other handlers to see this //
    return false;
}
