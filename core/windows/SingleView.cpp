// ------------------------------------ //
#include "SingleView.h"

#include "core/resources/Image.h"
#include "core/resources/Tags.h"

#include "core/components/SuperViewer.h"
#include "core/components/TagEditor.h"
#include "core/components/ImageListScroll.h"

#include "Exceptions.h"
#include "DualView.h"
#include "Common.h"

using namespace DV;
// ------------------------------------ //
SingleView::SingleView(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window),
    EditTagsButton("Edit Tags"), ShowImageInfoButton("View Image Info"),
    OpenInImporterButton("Open In Importer")
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

    //ImageToolbar->set_accel_group(get_accel_group());

    ImageToolbar->append(EditTagsButton);
    ImageToolbar->append(ShowImageInfoButton);
    ImageToolbar->append(OpenInImporterButton);
    
    ImageToolbar->show_all_children();

    EditTagsButton.signal_clicked().connect(sigc::mem_fun(*this,
            &SingleView::ToggleTagEditor));

    ShowImageInfoButton.signal_clicked().connect(sigc::mem_fun(*this,
            &SingleView::ToggleInfo));

    OpenInImporterButton.signal_clicked().connect(sigc::mem_fun(*this,
            &SingleView::OpenImporter));

    // This just doesn't want to work and it seems to be because Toolbar items don't have
    // activate signal
    //EditTagsButton.set_accel_path("<SingleImage-View>/Toolbar/EditTags", get_accel_group());
    //Gtk::AccelMap::add_entry("<SingleImage-View>/Toolbar/EditTags", GDK_KEY_T,
    //(Gdk::ModifierType)0);

    EditTagsButton.add_accelerator("clicked", get_accel_group(), GDK_KEY_T,
        Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);

    ShowImageInfoButton.add_accelerator("clicked", get_accel_group(), GDK_KEY_I,
        Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);

    BUILDER_GET_WIDGET(ImageProperties);
    ImageProperties->set_visible(false);

    auto tmpobject = builder->get_object("ImagePropertiesText");

    ImagePropertiesText = Glib::RefPtr<Gtk::TextBuffer>::cast_dynamic(tmpobject);

    LEVIATHAN_ASSERT(ImagePropertiesText, "Invalid .glade file");
}

SingleView::~SingleView(){

    Close();

    LOG_INFO("SingleView window destructed");
}
// ------------------------------------ //    
void SingleView::Open(std::shared_ptr<Image> image, std::shared_ptr<ImageListScroll> scroll){

    // Detach old image, if there is one //
    GUARD_LOCK();
    
    ReleaseParentHooks(guard);

    InCollection = scroll;

    ImageView->SetImage(image);
    ImageView->SetImageList(scroll);
    ImageView->RegisterSetImageNotify([&]()
        {
            UpdateImageNumber();

            // Update properties //
            if(ImageProperties->get_visible()){
        
                _LoadImageInfo();
            }

            GUARD_LOCK();
            OnTagsUpdated(guard);
        });

    UpdateImageNumber();
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

void SingleView::UpdateImageNumber(){

    DualView::IsOnMainThreadAssert();

    auto img = ImageView->GetImage();

    std::string title;

    if(!InCollection || !img){

        title = img ? img->GetName() : std::string("no image open");
        set_title(title + " | DualView++");
        
    } else {

        // img is valig here
        const auto desc = InCollection->GetDescriptionStr();
        auto collectionBrowse = InCollection;

        auto alive = GetAliveMarker();

        DualView::Get().QueueDBThreadFunction([=](){

                std::string title;

                if(collectionBrowse->SupportsRandomAccess() && collectionBrowse->HasCount()){
            
                    std::stringstream stream;
                    stream << (collectionBrowse->GetImageIndex(*img) + 1) << "/" <<
                        InCollection->GetCount() << " in " << desc << " image: " <<
                        img->GetName();
            
                    title = stream.str();
            
                } else {

                    std::stringstream stream;
                    stream << "image in " << desc << " image: " << img->GetName();
            
                    title = stream.str();
                }

                DualView::Get().InvokeFunction([=](){
                        set_title(title + " | DualView++");
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

void SingleView::ToggleInfo(){

    if(ImageProperties->get_visible()){

        // Hide //
        ImageProperties->hide();
        
    } else {

        // Make visible //
        ImageProperties->show();

        ImagePropertiesText->set_text("reading properties");
        _LoadImageInfo();
    }
}

void SingleView::_LoadImageInfo(){

    DualView::IsOnMainThreadAssert();

    auto alive = GetAliveMarker();
    auto img = ImageView->GetImage();
        
    // Load data //
    DualView::Get().QueueDBThreadFunction([=](){

            std::string ResourcePath = img->GetResourcePath();
            std::string ResourceName = img->GetName();
            std::string Extension = img->GetExtension();

            const auto IsPrivate = img->GetIsPrivate() ? "true" : "false";
    
            std::string AddDate = img->GetAddDateStr();

            std::string LastView = img->GetLastViewStr();

            std::string ImportLocation = img->GetFromFile();

            std::string Hash;

            int Height = img->GetHeight();
            int Width = img->GetWidth();
            auto id = img->GetID();

            try{

                Hash = img->GetHash();
                    
            } catch(const Leviathan::InvalidState&){

                // Hash is not ready //
                Hash = "not calculated yet";
            }

            DualView::Get().InvokeFunction([=](){

                    INVOKE_CHECK_ALIVE_MARKER(alive);

                    std::stringstream stream;
                    stream << "ID: " << id <<
                        "\nHash: " << Hash <<
                        "\nName: " << ResourceName <<
                        "\nExtension: " << Extension << " is private: " << IsPrivate <<
                        " dimensions: " << Width << "x" << Height <<
                        "\nPath: " << ResourcePath <<
                        "\nImported from: " << ImportLocation <<
                        "\nAdded: " << AddDate <<
                        "\nLast View: " << LastView;
                        
                    ImagePropertiesText->set_text(stream.str());
                });
        });
}
// ------------------------------------ //
void SingleView::OpenImporter(){

    DualView::Get().OpenImporter({ ImageView->GetImage() });
}
