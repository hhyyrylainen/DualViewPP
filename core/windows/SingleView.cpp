// ------------------------------------ //
#include "SingleView.h"

#include "core/resources/Image.h"
#include "core/resources/Tags.h"


#include "Exceptions.h"
#include "DualView.h"
#include "Common.h"

using namespace DV;
// ------------------------------------ //
SingleView::SingleView(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));

    try{
        
        builder->get_widget_derived("ImageView", ImageView, nullptr,
            SuperViewer::ENABLED_EVENTS::ALL, false);
        
    } catch(const Leviathan::InvalidArgument &e){

        LOG_WARNING("SingleView: failed to create SuperViewer, exception: ");
        e.PrintToLog();
        throw;
    }
    
    LEVIATHAN_ASSERT(ImageView, "Invalid SingleView .glade file");

    builder->get_widget("Tags", TagsLabel);
    LEVIATHAN_ASSERT(TagsLabel, "Invalid .glade file");
}

SingleView::~SingleView(){

    Close();

    LOG_INFO("SingleView window destructed");
}
// ------------------------------------ //    
void SingleView::Open(std::shared_ptr<Image> image){

    ImageView->SetImage(image);
    OnTagsUpdated();
}

void SingleView::OnTagsUpdated(){

    auto img = ImageView->GetImage();

    if(!img || !img->GetTags()){

        // Reset tags and block editing //
        TagsLabel->set_text("");
        
    } else {

        // Set tags //
        auto isalive = GetAliveMarker();
        auto tags = img->GetTags();

        // Set to editor //
        TagsLabel->set_text("Tags loading...");

        DualView::Get().QueueDBThreadFunction([this, isalive, tags](){

                std::string tagstr = tags->TagsAsString("; ");
    
                DualView::Get().InvokeFunction([this, isalive, tagstr](){

                        INVOKE_CHECK_ALIVE_MARKER(isalive);
            
                        TagsLabel->set_text(tagstr);
                    });
            });
    }
}
// ------------------------------------ //
void SingleView::_OnClose(){

    LOG_INFO("SingleView window closed");

}

