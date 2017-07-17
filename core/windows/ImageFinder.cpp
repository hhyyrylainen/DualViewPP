// ------------------------------------ //
#include "ImageFinder.h"

#include "DualView.h"
#include "Database.h"

#include "core/components/SuperContainer.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //
ImageFinder::ImageFinder(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));

    builder->get_widget_derived("FoundImageContainer", Container);
    LEVIATHAN_ASSERT(Container, "Invalid .glade file");

    BUILDER_GET_WIDGET(MainSearchBar);
    MainSearchBar->signal_changed().connect(sigc::mem_fun(*this,
            &ImageFinder::OnSearchChanged));

    BUILDER_GET_WIDGET(SearchActiveSpinner);

    BUILDER_GET_WIDGET(FoundImageCountLabel);
    
}

ImageFinder::~ImageFinder(){

    Close();
}

void ImageFinder::_OnClose(){

    // Close db queries if possible
}
// ------------------------------------ //
void ImageFinder::OnSearchChanged(){

    DualView::IsOnMainThreadAssert();
    
    _SetSearchingState(true);
}
// ------------------------------------ //
void ImageFinder::_SetSearchingState(bool active){

    SearchActiveSpinner->property_active() = active;

    if(active)
        FoundImageCountLabel->set_text("Searching ...");       
}



