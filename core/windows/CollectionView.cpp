// ------------------------------------ //
#include "CollectionView.h"

#include "core/components/SuperContainer.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //
CollectionView::CollectionView(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{

    signal_delete_event().connect(sigc::mem_fun(*this, &CollectionView::_OnClose));

    signal_unmap().connect(sigc::mem_fun(*this, &CollectionView::_OnHidden));
    signal_map().connect(sigc::mem_fun(*this, &CollectionView::_OnShown));
    
    builder->get_widget_derived("ImageContainer", Container);
    LEVIATHAN_ASSERT(Container, "Invalid .glade file");
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
// ------------------------------------ //
void CollectionView::_OnShown(){

    // Load items //
    std::vector<std::shared_ptr<Image>> images;
    
    for(int i = 0; i < 30; ++i)
        images.push_back(Image::Create("/home/hhyyrylainen/690806.jpg"));

    Container->SetShownItems(images.begin(), images.end());
}

void CollectionView::_OnHidden(){

    // Explicitly unload items //
    
}


