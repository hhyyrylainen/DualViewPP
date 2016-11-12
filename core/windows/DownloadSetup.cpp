// ------------------------------------ //
#include "DownloadSetup.h"

#include "core/resources/Tags.h"

#include "core/components/SuperViewer.h"
#include "core/components/TagEditor.h"
#include "core/components/FolderSelector.h"
#include "core/components/SuperContainer.h"

#include "core/DownloadManager.h"
#include "core/DualView.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //
DownloadSetup::DownloadSetup(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));

    builder->get_widget("OKButton", OKButton);
    LEVIATHAN_ASSERT(OKButton, "Invalid .glade file");

    builder->get_widget_derived("ImageDLSelector", ImageSelection);
    LEVIATHAN_ASSERT(ImageSelection, "Invalid .glade file");

    builder->get_widget_derived("FolderSelector", TargetFolder);
    LEVIATHAN_ASSERT(TargetFolder, "Invalid .glade file");

    builder->get_widget_derived("CollectionTags", CollectionTagEditor);
    LEVIATHAN_ASSERT(CollectionTagEditor, "Invalid .glade file");

#ifdef DV_BUILDER_WORKAROUND

    builder->get_widget_derived("CurrentImage", CurrentImage);
    ImageView->Init(nullptr, SuperViewer::ENABLED_EVENTS::ALL, false);
#else
    
    builder->get_widget_derived("CurrentImage", CurrentImage, nullptr,
        SuperViewer::ENABLED_EVENTS::ALL, false);

#endif // DV_BUILDER_WORKAROUND
    LEVIATHAN_ASSERT(CurrentImage, "Invalid .glade file");    

    builder->get_widget_derived("CurrentImageEditor", CurrentImageEditor);
    LEVIATHAN_ASSERT(CurrentImageEditor, "Invalid .glade file");

    CollectionTags = std::make_shared<TagCollection>();

    CollectionTagEditor->SetEditedTags({CollectionTags});


    builder->get_widget("URLEntry", URLEntry);
    LEVIATHAN_ASSERT(URLEntry, "Invalid .glade file");

    URLEntry->signal_activate().connect(sigc::mem_fun(*this, &DownloadSetup::OnURLChanged));

    URLEntry->signal_changed().connect(sigc::mem_fun(*this, &DownloadSetup::OnInvalidateURL));

    builder->get_widget("DetectedSettings", DetectedSettings);
    LEVIATHAN_ASSERT(DetectedSettings, "Invalid .glade file");

    builder->get_widget("URLCheckSpinner", URLCheckSpinner);
    LEVIATHAN_ASSERT(URLCheckSpinner, "Invalid .glade file");
}

DownloadSetup::~DownloadSetup(){

    Close();
}
// ------------------------------------ //
void DownloadSetup::_OnClose(){


}
// ------------------------------------ //
void DownloadSetup::OnURLChanged(){
    
    DetectedSettings->set_text("Checking for valid URL, please wait.");
    URLCheckSpinner->property_active() = true;

    try{
        
        auto scan = std::make_shared<PageScanJob>(URLEntry->get_text());

        DualView::Get().GetDownloadManager().QueueDownload(scan);

    } catch(const Leviathan::InvalidArgument&){

        // Invalid url //
        UrlCheckFinished(false, "website not supported");
    }
}

void DownloadSetup::OnInvalidateURL(){

    DetectedSettings->set_text("URL changed, accept it to update.");
    URLCheckSpinner->property_active() = false;
}

void DownloadSetup::UrlCheckFinished(bool wasvalid, const std::string &message){

    DualView::IsOnMainThreadAssert();

    URLCheckSpinner->property_active() = false;

    if(!wasvalid){

        DetectedSettings->set_text("Invalid URL: " + message);
        return;
    }
}

