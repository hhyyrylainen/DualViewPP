// ------------------------------------ //
#include "SingleView.h"

#include "core/resources/Image.h"
#include "core/resources/Tags.h"

#include "core/components/SuperViewer.h"
#include "core/components/TagEditor.h"

#include "Exceptions.h"
#include "DualView.h"
#include "Common.h"

using namespace DV;
// ------------------------------------ //
SingleView::SingleView(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window),
    EditTagsButton("Edit Tags")
{
    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));

    try{

    #ifdef DV_BUILDER_WORKAROUND

        builder->get_widget_derived("ImageView", ImageView);
        ImageView->Init(nullptr, SuperViewer::ENABLED_EVENTS::ALL, false);
    #else
        
        builder->get_widget_derived("ImageView", ImageView, nullptr,
            SuperViewer::ENABLED_EVENTS::ALL, false);

    #endif // DV_BUILDER_WORKAROUND
        
    } catch(const Leviathan::InvalidArgument &e){

        LOG_WARNING("SingleView: failed to create SuperViewer, exception: ");
        e.PrintToLog();
        throw;
    }
    
    LEVIATHAN_ASSERT(ImageView, "Invalid SingleView .glade file");

    builder->get_widget("Tags", TagsLabel);
    LEVIATHAN_ASSERT(TagsLabel, "Invalid .glade file");

    builder->get_widget("ImageSize", ImageSize);
    LEVIATHAN_ASSERT(ImageSize, "Invalid .glade file");

    builder->get_widget_derived("ImageTags", ImageTags);
    LEVIATHAN_ASSERT(ImageTags, "Invalid .glade file");

    // Fill the toolbar //
    Gtk::Toolbar* ImageToolbar;
    builder->get_widget("ImageToolbar", ImageToolbar);
    LEVIATHAN_ASSERT(ImageToolbar, "Invalid .glade file");

    ImageToolbar->append(EditTagsButton, sigc::mem_fun(*this,
            &SingleView::ToggleTagEditor));
    
    ImageToolbar->show_all_children();

}

SingleView::~SingleView(){

    Close();

    LOG_INFO("SingleView window destructed");
}
// ------------------------------------ //    
void SingleView::Open(std::shared_ptr<Image> image){

    // Detach old image, if there is one //
    GUARD_LOCK();
    
    ReleaseParentHooks(guard);

    ImageView->SetImage(image);
    OnTagsUpdated(guard);
}
// ------------------------------------ //
void SingleView::OnNotified(Lock &ownlock, Leviathan::BaseNotifierAll* parent,
    Lock &parentlock)
{
    OnTagsUpdated(ownlock);
}

void SingleView::OnTagsUpdated(Lock &guard){

    auto img = ImageView->GetImage();

    if(!img){

        ImageSize->set_text("No image");
        TagsLabel->set_text("");
        return;
        
    } else {

        ImageSize->set_text(Convert::ToString(img->GetWidth()) + "x" +
            Convert::ToString(img->GetHeight()));
    }

    auto tags = img ? img->GetTags() : nullptr;

    if(ImageTags->get_visible()){

        ImageTags->SetEditedTags({ tags });
    }

    // Start listening for changes on the image //
    if(!IsConnectedTo(img.get(), guard))
        ConnectToNotifier(guard, img.get());

    if(!tags){

        TagsLabel->set_text("");
        
    } else {
        
        // Set tags //
        auto isalive = GetAliveMarker();
        
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
// ------------------------------------ //
void SingleView::ToggleTagEditor(){

    if(ImageTags->get_visible()){

        ImageTags->SetEditedTags({});
        ImageTags->hide();
        
    } else {
        
        ImageTags->show();
        auto img = ImageView->GetImage();
        ImageTags->SetEditedTags({ img ? img->GetTags() : nullptr});
    }
}
