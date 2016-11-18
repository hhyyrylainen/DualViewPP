// ------------------------------------ //
#include "DownloadSetup.h"

#include "core/resources/Tags.h"

#include "core/components/SuperViewer.h"
#include "core/components/TagEditor.h"
#include "core/components/FolderSelector.h"
#include "core/components/SuperContainer.h"

#include "core/DownloadManager.h"
#include "core/PluginManager.h"
#include "core/DualView.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //
DownloadSetup::DownloadSetup(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));
    
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

    BUILDER_GET_WIDGET(URLEntry);

    URLEntry->signal_activate().connect(sigc::mem_fun(*this, &DownloadSetup::OnURLChanged));

    URLEntry->signal_changed().connect(sigc::mem_fun(*this, &DownloadSetup::OnInvalidateURL));

    BUILDER_GET_WIDGET(DetectedSettings);

    BUILDER_GET_WIDGET(URLCheckSpinner);

    BUILDER_GET_WIDGET(OKButton);

    BUILDER_GET_WIDGET(PageRangeLabel);

    BUILDER_GET_WIDGET(ScanPages);

    BUILDER_GET_WIDGET(PageScanSpinner);
}

DownloadSetup::~DownloadSetup(){

    Close();
}
// ------------------------------------ //
void DownloadSetup::_OnClose(){


}
// ------------------------------------ //
void DownloadSetup::AddSubpage(const std::string &url){

    for(const auto& existing : PagesToScan){

        if(existing == url)
            return;
    }

    PagesToScan.push_back(url);
}
// ------------------------------------ //
void DownloadSetup::OnURLChanged(){

    if(UrlBeingChecked)
        return;
    
    UrlBeingChecked = true;
    _SetState(STATE::CHECKING_URL);
    
    DetectedSettings->set_text("Checking for valid URL, please wait.");

    std::string str = URLEntry->get_text();
    CurrentlyCheckedURL = str;

    // Find plugin for URL //
    auto scanner = DualView::Get().GetPluginManager().GetScannerForURL(str);

    if(!scanner){

        UrlCheckFinished(false, "No plugin found that supports input url");
        return;
    }

    // Link rewrite //
    if(scanner->UsesURLRewrite()){

        str = scanner->RewriteURL(str);
        URLEntry->set_text(str.c_str());
    }

    try{
        
        auto scan = std::make_shared<PageScanJob>(str);

        auto alive = GetAliveMarker();
        
        scan->SetFinishCallback([this, alive, scan, str](DownloadJob &job, bool success){

                // Store the pages //
                if(success){
                    
                    ScanResult& result = scan->GetResult();

                    // Add the main page //
                    AddSubpage(str);

                    for(const auto &page : result.PageLinks)
                        AddSubpage(page);
                }

                DualView::Get().InvokeFunction([this, alive, success, scan](){

                        INVOKE_CHECK_ALIVE_MARKER(alive);

                        // Finished //
                        if(!success){

                            UrlCheckFinished(false, "URL scanning failed");
                            return;
                        }
                
                        DetectedSettings->set_text("All Good");
                        UrlCheckFinished(true, "");
                    });
            });

        DualView::Get().GetDownloadManager().QueueDownload(scan);

    } catch(const Leviathan::InvalidArgument&){

        // Invalid url //
        UrlCheckFinished(false, "website not supported");
    }

    UrlBeingChecked = false;
}

void DownloadSetup::OnInvalidateURL(){

    // This gets called if an url rewrite happens in OnURLChanged
    if(UrlBeingChecked)
        return;

    _SetState(STATE::URL_CHANGED);
    DetectedSettings->set_text("URL changed, accept it to update.");
}

void DownloadSetup::UrlCheckFinished(bool wasvalid, const std::string &message){

    DualView::IsOnMainThreadAssert();

    UrlBeingChecked = false;

    if(!wasvalid){

        DetectedSettings->set_text("Invalid URL: " + message);
        _SetState(STATE::URL_CHANGED);
        return;
    }

    // The scanner settings are updated when the state is set to STATE::URL_OK automatically //
    _SetState(STATE::URL_OK);
}
// ------------------------------------ //
void DownloadSetup::_SetState(STATE newstate){

    if(State == newstate)
        return;

    State = newstate;
    auto alive = GetAliveMarker();
    
    DualView::Get().InvokeFunction([this, alive](){

            INVOKE_CHECK_ALIVE_MARKER(alive);

            _UpdateWidgetStates();
        });
}

void DownloadSetup::_UpdateWidgetStates(){

    DualView::IsOnMainThreadAssert();

    URLCheckSpinner->property_active() = State == STATE::CHECKING_URL; 

    // Set button states //

    if(State == STATE::URL_OK){

        ScanPages->set_sensitive(true);
        
    } else {

        ScanPages->set_sensitive(false);
    }

    switch(State){
    case STATE::CHECKING_URL:
    {

    }
    break;
    case STATE::URL_CHANGED:
    {


    }
    break;
    case STATE::URL_OK:
    {
        // Update page scan state //
        if(PagesToScan.empty()){

            PageRangeLabel->set_text("0"); 
            
        } else {

            PageRangeLabel->set_text("1-" + Convert::ToString(PagesToScan.size())); 
        }
    }
    break;
    

    }
}

