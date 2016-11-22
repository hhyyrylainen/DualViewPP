// ------------------------------------ //
#include "DownloadSetup.h"

#include "core/resources/Tags.h"
#include "core/resources/InternetImage.h"
#include "core/resources/Folder.h"
#include "core/resources/NetGallery.h"

#include "core/components/SuperViewer.h"
#include "core/components/TagEditor.h"
#include "core/components/FolderSelector.h"
#include "core/components/SuperContainer.h"

#include "core/DownloadManager.h"
#include "core/PluginManager.h"
#include "core/DualView.h"
#include "core/Database.h"

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

    OKButton->signal_clicked().connect(sigc::mem_fun(*this,
            &DownloadSetup::OnUserAcceptSettings));

    BUILDER_GET_WIDGET(PageRangeLabel);

    BUILDER_GET_WIDGET(ScanPages);
    ScanPages->signal_clicked().connect(sigc::mem_fun(*this,
            &DownloadSetup::StartPageScanning));

    BUILDER_GET_WIDGET(PageScanSpinner);

    BUILDER_GET_WIDGET(CurrentScanURL);

    BUILDER_GET_WIDGET(PageScanProgress);

    BUILDER_GET_WIDGET(TargetCollectionName);

    BUILDER_GET_WIDGET(MainStatusLabel);

    BUILDER_GET_WIDGET(SelectOnlyOneImage);

    // Set all the editor controls read only
    _UpdateWidgetStates();
}

DownloadSetup::~DownloadSetup(){

    Close();
}
// ------------------------------------ //
void DownloadSetup::_OnClose(){

    
}
// ------------------------------------ //
void DownloadSetup::OnUserAcceptSettings(){

    if(State != STATE::URL_OK){

        LOG_ERROR("DownloadSetup: trying to accept in not URL_OK state");
        return;
    }

    if(!IsReadyToDownload())
        return;

    // Create a DownloadCollection and add that to the database
    const auto selected = GetSelectedImages();

    // Cache all images that are already downloaded
    DualView::Get().QueueWorkerFunction([selected](){

            for(const auto& image : selected){

                image->SaveFileToDisk();
            }
        });
    
    auto gallery = std::make_shared<NetGallery>(CurrentlyCheckedURL);

    if(CollectionTags->HasTags()){
        
        gallery->SetTags(CollectionTags->TagsAsString(";"));
        
        CollectionTags = std::make_shared<TagCollection>();
        CollectionTagEditor->SetEditedTags({CollectionTags});
    }

    if(!TargetFolder->GetFolder()->IsRoot()){

        gallery->SetTargetPath(TargetFolder->GetPath());
        
        TargetFolder->GoToRoot();
    }
    
    // Save the net gallery to the databse
    // (which also allows the DownloadManager to pick it up)
    DualView::Get().GetDatabase().InsertNetGallery(gallery);
    gallery->AddFilesToDownload(selected);

    // Remove the added from the list //
    for(size_t i = 0; i < ImageObjects.size(); ){

        bool removed = false;

        for(const auto& added : selected){
            if(ImageObjects[i].get() == added.get()){

                ImageObjects.erase(ImageObjects.begin() + i);
                ImagesToDownload.erase(ImagesToDownload.begin() + i);
                removed = true;
                break;
            }
        }

        if(!removed)
            ++i;
    }


    // We are done

    // If there are leftover images allow adding those to another collection
    if(ImageObjects.empty()){
        
        close();
        return;
    }

    // There are still some stuff //
    auto dialog = Gtk::MessageDialog(*this,
        "Added Some Images From This Internet Resource",
        false,
        Gtk::MESSAGE_INFO,
        Gtk::BUTTONS_OK,
        true 
    );
    
    dialog.set_secondary_text("You can either select the remaining images and add them also. "
        "Or you can close this window to discard the rest of the images");
    dialog.run();

    ImageSelection->SetShownItems(ImageObjects.begin(), ImageObjects.end());
    UpdateEditedImages();
}
// ------------------------------------ //
void DownloadSetup::AddSubpage(const std::string &url){

    for(const auto& existing : PagesToScan){

        if(existing == url)
            return;
    }

    PagesToScan.push_back(url);
}

void DownloadSetup::OnFoundContent(const ScanFoundImage &content){

    DualView::IsOnMainThreadAssert();
    
    for(auto& existinglink : ImagesToDownload){

        if(existinglink == content){
            existinglink.Merge(content);
            return;
        }
    }
    
    try{
        
        ImageObjects.push_back(InternetImage::Create(content, false));
        
    } catch(const Leviathan::InvalidArgument &e){

        LOG_ERROR("DownloadSetup: failed to create InternetImage for link because url "
            "is invalid, link: " + content.URL + ", exception: ");
        e.PrintToLog();
        return;
    }

    ImagesToDownload.push_back(content);
    
    // Add it to the selectable content //
    ImageSelection->AddItem(ImageObjects.back(), std::make_shared<ItemSelectable>(
            std::bind(&DownloadSetup::OnItemSelected, this, std::placeholders::_1)));

    LOG_INFO("DownloadSetup added new image: " + content.URL);
}
// ------------------------------------ //
void DownloadSetup::OnItemSelected(ListItem &item){

    // Deselect others if only one is wanted //
    if(SelectOnlyOneImage->get_active() && item.IsSelected()){

        // Deselect all others //
        ImageSelection->DeselectAllExcept(&item);
    }

    UpdateEditedImages();
}

void DownloadSetup::UpdateEditedImages(){

    const auto result = GetSelectedImages();
    
    // Preview image //
    if(result.empty()){
        
        CurrentImage->RemoveImage();
        
    } else {
        
        CurrentImage->SetImage(result.front());
    }

    // Tag editing //
    std::vector<std::shared_ptr<TagCollection>> tagstoedit;
    
    for(const auto& image : result){

        tagstoedit.push_back(image->GetTags());
    }

    CurrentImageEditor->SetEditedTags(tagstoedit);

    UpdateReadyStatus();
}

std::vector<std::shared_ptr<InternetImage>> DownloadSetup::GetSelectedImages(){

    std::vector<std::shared_ptr<InternetImage>> result;

    std::vector<std::shared_ptr<ResourceWithPreview>> SelectedItems;
    
    ImageSelection->GetSelectedItems(SelectedItems);

    for(const auto& preview : SelectedItems){

        auto asImage = std::dynamic_pointer_cast<InternetImage>(preview);

        if(!asImage){

            LOG_WARNING("DownloadSetup: SuperContainer has something that "
                "isn't InternetImage");
            continue;
        }
        
        result.push_back(asImage);
    }

    return result;
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

                    // Set the title //
                    if(!result.PageTitle.empty())
                        SetTargetCollectionName(result.PageTitle);
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
//! Data for DownloadSetup::StartPageScanning
struct DV::SetupScanQueueData{

    std::string MainReferrer;

    std::vector<std::string> PagesToScan;
    size_t CurrentPageToScan = 0;

    ScanResult Scans;
};

void DV::QueueNextThing(std::shared_ptr<SetupScanQueueData> data, DownloadSetup* setup,
    IsAlive::AliveMarkerT alive, std::shared_ptr<PageScanJob> scanned)
{
    if(scanned){

        data->Scans.Combine(scanned->GetResult());
    }
    
    auto finished = [=](){

        DualView::IsOnMainThreadAssert();
        INVOKE_CHECK_ALIVE_MARKER(alive);

        LOG_INFO("Finished Scanning");

        // Add the content //
        for(const auto& content : data->Scans.ContentLinks)
            setup->OnFoundContent(content);

        // Add new subpages //
        for(const auto& page : data->Scans.PageLinks)
            setup->AddSubpage(page);

        setup->PageScanProgress->set_value(1.0);
        setup->_SetState(DownloadSetup::STATE::URL_OK);
    };

    if(data->PagesToScan.size() <= data->CurrentPageToScan){

        LOG_INFO("DownloadSetup scan finished, result:");
        data->Scans.PrintInfo();
        DualView::Get().InvokeFunction(finished);                     
        return;
    }

    LOG_INFO("DownloadSetup running scanning task " +
        Convert::ToString(data->CurrentPageToScan + 1) + "/" +
        Convert::ToString(data->PagesToScan.size()));


    float progress = static_cast<float>(data->CurrentPageToScan) /
        data->PagesToScan.size();
        
    const auto str = data->PagesToScan[data->CurrentPageToScan];
    ++data->CurrentPageToScan;

    // Update status //
    DualView::Get().InvokeFunction([setup, alive, str, progress](){

            INVOKE_CHECK_ALIVE_MARKER(alive);

            // Scanned link //
            setup->CurrentScanURL->set_uri(str);
            setup->CurrentScanURL->set_label(str);
            setup->CurrentScanURL->set_sensitive(true);

            // Progress bar //
            setup->PageScanProgress->set_value(progress);
        });

                
    try{
        auto scan = std::make_shared<PageScanJob>(str, data->MainReferrer);
        // Queue next call //
        scan->SetFinishCallback(std::bind(&DV::QueueNextThing, data, setup, alive, scan));

        DualView::Get().GetDownloadManager().QueueDownload(scan);

    } catch(const Leviathan::InvalidArgument&){

        LOG_ERROR("DownloadSetup invalid url to scan: " + str);
    }
}

void DownloadSetup::StartPageScanning(){

    DualView::IsOnMainThreadAssert();

    auto expectedstate = STATE::URL_OK;
    if(!State.compare_exchange_weak(expectedstate, STATE::SCANNING_PAGES,
        std::memory_order_release,
            std::memory_order_acquire))
    {
        LOG_ERROR("Tried to enter DownloadSetup::StartPageScanning while not in URL_OK state");
        return;
    }

    _UpdateWidgetStates();
    
    auto alive = GetAliveMarker();

    auto data = std::make_shared<SetupScanQueueData>();
    data->PagesToScan = PagesToScan;
    data->MainReferrer = CurrentlyCheckedURL;

    DualView::Get().QueueWorkerFunction(std::bind(&QueueNextThing, data, this, alive,
            nullptr));
}

// ------------------------------------ //
void DownloadSetup::SetTargetCollectionName(const std::string &str){

    TargetCollectionName->set_text(str);
}
// ------------------------------------ //
void DownloadSetup::_SetState(STATE newstate){

    if(State == newstate)
        return;

    State = newstate;
    auto alive = GetAliveMarker();
    
    DualView::Get().RunOnMainThread([this, alive](){

            INVOKE_CHECK_ALIVE_MARKER(alive);

            _UpdateWidgetStates();
        });
}

void DownloadSetup::_UpdateWidgetStates(){

    DualView::IsOnMainThreadAssert();

    // Spinners //
    URLCheckSpinner->property_active() = State == STATE::CHECKING_URL;
    PageScanSpinner->property_active() = State == STATE::SCANNING_PAGES; 

    // Set button states //
    ScanPages->set_sensitive(State == STATE::URL_OK);

    if(State == STATE::URL_OK){

        TargetFolder->set_sensitive(true);
        CollectionTagEditor->set_sensitive(true);
        CurrentImageEditor->set_sensitive(true);
        CurrentImage->set_sensitive(true);
        OKButton->set_sensitive(true);
        ImageSelection->set_sensitive(true);
        TargetCollectionName->set_sensitive(true);
        
    } else {
        
        TargetFolder->set_sensitive(false);
        CollectionTagEditor->set_sensitive(false);
        CurrentImageEditor->set_sensitive(false);
        CurrentImage->set_sensitive(false);
        OKButton->set_sensitive(false);
        ImageSelection->set_sensitive(false);
        TargetCollectionName->set_sensitive(false);
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

        // Update main status //
        UpdateReadyStatus();

    }
    break;
    case STATE::SCANNING_PAGES:
    {
        

    }
    break;
    

    }
}

void DownloadSetup::UpdateReadyStatus(){
    
    const auto selected = ImageSelection->CountSelectedItems();
    const auto total = ImageObjects.size();

        
    bool ready = IsReadyToDownload(); 
        
    MainStatusLabel->set_text((ready ? "Ready" : "Not ready") + std::string(
            " to download " + Convert::ToString(selected) + " (out of " +
            Convert::ToString(total) + ") images to \"" + TargetCollectionName->get_text()
            + "\""));
}

bool DownloadSetup::IsReadyToDownload() const{

    if(State != STATE::URL_OK)
        return false;
    
    const auto selected = ImageSelection->CountSelectedItems();
    const auto total = ImageObjects.size();

    return selected > 0 && selected <= total;
}

