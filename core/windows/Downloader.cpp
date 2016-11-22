// ------------------------------------ //
#include "Downloader.h"

#include "core/components/DLListItem.h"

#include "core/resources/NetGallery.h"

#include "core/DualView.h"
#include "core/Database.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //
Downloader::Downloader(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &Downloader::_OnClose));

    signal_unmap().connect(sigc::mem_fun(*this, &Downloader::_OnHidden));
    signal_map().connect(sigc::mem_fun(*this, &Downloader::_OnShown));

    builder->get_widget("DLList", DLWidgets);
    LEVIATHAN_ASSERT(DLWidgets, "Invalid .glade file");


    Gtk::Button* AddNewLink;
    builder->get_widget("AddNewLink", AddNewLink);
    LEVIATHAN_ASSERT(AddNewLink, "Invalid .glade file");
    
    AddNewLink->signal_pressed().connect(sigc::mem_fun(*this,
            &Downloader::_OpenNewDownloadSetup));


    BUILDER_GET_WIDGET(StartDownloadButton);
    StartDownloadButton->signal_pressed().connect(sigc::mem_fun(*this,
            &Downloader::_ToggleDownloadThread));

    BUILDER_GET_WIDGET(DLStatusLabel);
    BUILDER_GET_WIDGET(DLSpinner);
}

Downloader::~Downloader(){

    WaitForDownloadThread();
}

// ------------------------------------ //
bool Downloader::_OnClose(GdkEventAny* event){

    // Ask user to stop downloads //

    StopDownloadThread();

    // Just hide it //
    hide();

    return true;
}

void Downloader::_OnShown(){
    
    // Load items if not already loaded //
    const auto itemids = DualView::Get().GetDatabase().SelectNetGalleryIDs(true);


    for(auto id : itemids){

        // Skip already added ones //
        bool added = false;

        for(const auto &existing : DLList){

            if(existing->GetGallery()->GetID() == id){

                added = true;
                break;
            }
        }

        if(added)
            continue;

        AddNetGallery(DualView::Get().GetDatabase().SelectNetGalleryByIDAG(id));
    }
}

void Downloader::_OnHidden(){

    // Ask user whether downloads should be paused //
    
}
// ------------------------------------ //
void Downloader::AddNetGallery(std::shared_ptr<NetGallery> gallery){

    if(!gallery){

        LOG_ERROR("Downloader trying to add null NetGallery");
        return;
    }

    auto item = std::make_shared<DLListItem>(gallery);
    DLList.push_back(item);

    DLWidgets->add(*item);
    item->show();            
}
// ------------------------------------ //
void Downloader::_OpenNewDownloadSetup(){
    DualView::Get().OpenDownloadSetup();
}
// ------------------------------------ //
void Downloader::StartDownloadThread(){

    if(RunDownloadThread)
        return;
    
    // Make sure the thread isn't running 
    WaitForDownloadThread();

    RunDownloadThread = true;
    
    DownloadThread = std::thread(&Downloader::_RunDownloadThread, this);
}

void Downloader::StopDownloadThread(){

    RunDownloadThread = false;
}

void Downloader::WaitForDownloadThread(){

    if(RunDownloadThread)
        StopDownloadThread();

    NotifyDownloadThread.notify_all();

    if(DownloadThread.joinable())
        DownloadThread.join();
}
// ------------------------------------ //
void Downloader::_RunDownloadThread(){

    std::unique_lock<std::mutex> lock(DownloadThreadMutex);

    while(RunDownloadThread){

        
        
        
        NotifyDownloadThread.wait_for(lock, std::chrono::milliseconds(10));
    }
}
// ------------------------------------ //
void Downloader::_ToggleDownloadThread(){

    if(RunDownloadThread){
        
        StopDownloadThread();
        StartDownloadButton->set_label("Start Download");
        DLStatusLabel->set_text("Not downloading");
        
    } else {

        StartDownloadThread();
        StartDownloadButton->set_label("Stop Download Thread");
        DLStatusLabel->set_text("Downloader thread waiting for work");
    }
}
// ------------------------------------ //


