// ------------------------------------ //
#include "Downloader.h"

#include "core/components/DLListItem.h"

#include "core/resources/NetGallery.h"
#include "core/resources/Image.h"
#include "core/resources/Tags.h"
#include "core/resources/Collection.h"

#include "core/DualView.h"
#include "core/Database.h"
#include "core/Settings.h"
#include "core/DownloadManager.h"
#include "core/ChangeEvents.h"

#include "leviathan/Common/StringOperations.h"

#include "Common.h"

#include <boost/filesystem.hpp>

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
    BUILDER_GET_WIDGET(DLProgress);

    // Listen for new download galleries //
    GUARD_LOCK();
    DualView::Get().GetEvents().RegisterForEvent(CHANGED_EVENT::NET_GALLERY_CREATED,
        this, guard);
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

void Downloader::OnNotified(Lock &ownlock, Leviathan::BaseNotifierAll* parent,
    Lock &parentlock)
{
    _OnShown();
}

void Downloader::_OnShown(){

    auto alive = GetAliveMarker();

    DualView::Get().QueueDBThreadFunction([=](){
    
            // Load items if not already loaded //
            const auto itemids = DualView::Get().GetDatabase().SelectNetGalleryIDs(true);

            DualView::Get().InvokeFunction([=](){

                    INVOKE_CHECK_ALIVE_MARKER(alive);
                    
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

                        // TODO: these objects could be created on the database thread
                        AddNetGallery(
                            DualView::Get().GetDatabase().SelectNetGalleryByIDAG(id));
                    }            
                });
        });
}

void Downloader::_OnHidden(){

    // Ask user whether downloads should be paused //
    StopDownloadThread();
}
// ------------------------------------ //
void Downloader::AddNetGallery(std::shared_ptr<NetGallery> gallery){

    if(!gallery){

        LOG_ERROR("Downloader trying to add null NetGallery");
        return;
    }

    auto item = std::make_shared<DLListItem>(gallery);

    // The item should always be destroyed before this
    item->SetRemoveCallback(std::bind(&Downloader::_OnRemoveListItem, this,
            std::placeholders::_1));
    
    DLList.push_back(item);
    

    DLWidgets->add(*item);
    item->show();            
}

void Downloader::_OnRemoveListItem(DLListItem &item){

    auto alive = GetAliveMarker();
    
    DualView::Get().InvokeFunction([=, toremove { &item }](){

            INVOKE_CHECK_ALIVE_MARKER(alive);

            for(auto iter = DLList.begin(); iter != DLList.end(); ++iter){

                if((*iter).get() == toremove){

                    (*iter)->hide();
                    DLWidgets->remove(**iter);
                    DLList.erase(iter);
                    break;
                }
            }
        });
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

    auto alive = GetAliveMarker();

    DualView::Get().RunOnMainThread([=](){

            INVOKE_CHECK_ALIVE_MARKER(alive);
            StartDownloadButton->set_label("Stop Download Thread");
        });
}

void Downloader::StopDownloadThread(){
    
    RunDownloadThread = false;

    auto alive = GetAliveMarker();

    DualView::Get().RunOnMainThread([=](){

            INVOKE_CHECK_ALIVE_MARKER(alive);
            StartDownloadButton->set_label("Start Download");
        });
}

void Downloader::WaitForDownloadThread(){

    if(RunDownloadThread)
        StopDownloadThread();

    NotifyDownloadThread.notify_all();

    if(DownloadThread.joinable())
        DownloadThread.join();
}
// ------------------------------------ //
std::shared_ptr<DLListItem> Downloader::GetNextSelectedGallery(){

    std::promise<std::shared_ptr<DLListItem>> result;

    DualView::Get().RunOnMainThread([&](){

            std::shared_ptr<DLListItem> foundGallery;

            for(auto& item : DLList){

                if(item->IsSelected()){

                    foundGallery = item;
                    break;
                }
            }

            result.set_value(foundGallery);
        });

    return result.get_future().get();
}

void Downloader::_DLFinished(std::shared_ptr<DLListItem> item){

    std::promise<bool> done;

    DualView::Get().RunOnMainThread([&](){

            DualView::Get().QueueDBThreadFunction([gallery { item->GetGallery()}](){

                    gallery->SetIsDownload(true);
                });

            _OnRemoveListItem(*item);

            done.set_value(true);
        });

    done.get_future().wait();
}

// ------------------------------------ //
void Downloader::_SetDLThreadStatus(const std::string &statusstr, bool spinneractive,
    float progress)
{

    auto alive = GetAliveMarker();

    DualView::Get().RunOnMainThread([=](){

            INVOKE_CHECK_ALIVE_MARKER(alive);

            if(!statusstr.empty())
                DLStatusLabel->set_text(statusstr);
            DLSpinner->property_active() = spinneractive;

            if(progress >= 0.f)
                DLProgress->set_value(progress);
        });
}

// ------------------------------------ //
//! Holds the state of a download
struct DV::DownloadProgressState{

    enum class STATE{

        INITIAL,

        //! Waiting to get a list of images to download
        WAITING_FOR_DB,

        DOWNLOADING_IMAGES,

        WAITING_FOR_HASHES,
        
        ENDED
    };

    DownloadProgressState(Downloader* downloader, std::shared_ptr<DLListItem> listitem,
        std::shared_ptr<NetGallery> gallery) :
        Loader(downloader), Gallery(gallery), Widget(listitem)
    {
        Widget->LockSelected(true);
        Loader->_SetDLThreadStatus("Downloading: " + gallery->GetTargetGalleryName(), true, 0);
    }

    ~DownloadProgressState(){

        if(Widget)
            Widget->LockSelected(false);
    }

    //! \returns True once done
    bool Tick(std::shared_ptr<DownloadProgressState> us){

        switch(state){
        case STATE::INITIAL:
        {

            DualView::Get().QueueDBThreadFunction([us](){

                    us->ImageList = DualView::Get().GetDatabase().SelectNetFilesFromGallery(
                        *us->Gallery);
                    
                    us->ImageListReady = true;
                });
            
            state = STATE::WAITING_FOR_DB;
            return false;
        }
        case STATE::WAITING_FOR_DB:
        {
            Loader->_SetDLThreadStatus("Waiting on Database", true, 0.05f);
            if(ImageListReady){

                state = STATE::DOWNLOADING_IMAGES;
            }
            
            return false;
        }
        case STATE::DOWNLOADING_IMAGES:
        {
            if(imagedl){

                // Wait for the download to finish //
                if(!imagedl->IsReady())
                    return false;

                if(imagedl->HasFailed()){

                    ++DLRetries;

                    if(DLRetries > DualView::Get().GetSettings().GetMaxDLRetries()){

                        Loader->_SetDLThreadStatus("Max retries reached for failed dl: "
                            + imagedl->GetURL(),
                            false, -1);
                        return false;
                    }

                    LOG_ERROR("Downloading failed (retrying) for URL: " + imagedl->GetURL());
                    Loader->_SetDLThreadStatus("Failed to download, retry number " +
                        Convert::ToString(DLRetries) + ", url: " + imagedl->GetURL(),
                        false, -1);
                    
                    return false;
                }
                
                DownloadedImages.push_back(Image::Create(imagedl->GetLocalFile(),
                        DownloadManager::ExtractFileName(imagedl->GetURL()),
                        imagedl->GetURL()));
                
                LOG_INFO("Successfully downloaded: " + imagedl->GetURL());
                LOG_INFO("Local path: " + imagedl->GetLocalFile());
                imagedl.reset();
                return false;
            }

            if(CurrentDownload >= ImageList.size()){

                // Finished downloading //
                state = STATE::WAITING_FOR_HASHES;
                return false;
            }
            
            const float progress = static_cast<float>(CurrentDownload) / ImageList.size();
            
            Loader->_SetDLThreadStatus("Downloading image #" +
                Convert::ToString(CurrentDownload + 1), true, 0.1f + (0.9f * progress));
            Widget->SetProgress(progress);

            // Download if the target file doesn't exist yet //
            const auto currentdl = ImageList[CurrentDownload];
            DLRetries = 0;
            
            const auto cachefile = DownloadManager::GetCachePathForURL(
                currentdl->GetFileURL());

            if(boost::filesystem::exists(cachefile)){

                LOG_INFO("Downloader: found locally cached version, using this instead of "
                    "the URL: " + currentdl->GetFileURL() + " file: " + cachefile);

                // Auto wanted path //
                const auto wantedpath = (boost::filesystem::path(
                        DualView::Get().GetSettings().GetStagingFolder()) /
                    currentdl->GetPreferredName()).string();

                std::string finalpath = wantedpath;

                if(!boost::filesystem::equivalent(cachefile, wantedpath)){
                
                    // Rename into target file //
                    auto path = DualView::MakePathUniqueAndShort(wantedpath);

                    boost::filesystem::rename(cachefile, path);

                    LEVIATHAN_ASSERT(boost::filesystem::exists(path), "Move file failed");

                    finalpath = path;
                }

               DownloadedImages.push_back(Image::Create(finalpath,
                       currentdl->GetPreferredName(), currentdl->GetFileURL()));

            } else {

                // Download it //
                imagedl = std::make_shared<ImageFileDLJob>(currentdl->GetFileURL(),
                    currentdl->GetPageReferrer());

                DualView::Get().GetDownloadManager().QueueDownload(imagedl);
            }
            
            ++CurrentDownload;
            
            return false;
        }
        case STATE::WAITING_FOR_HASHES:
        {
            Widget->SetProgress(1.0f);
            
            for(const auto image : DownloadedImages){

                if(!image->IsReady()){

                    Loader->_SetDLThreadStatus("Waiting for hash calculations to end",
                        true, 1.0f);

                    
                    return false;
                }
            }

            LOG_INFO("TODO: delete files in staging folder that already existed");
            state = STATE::ENDED;
            return false;
        }
        case STATE::ENDED:
        {
            // Queue import on a worker thread //
            Loader->_SetDLThreadStatus("Starting Import", false, 0);

            TagCollection tags;

            if(!Gallery->GetTagsString().empty()){

                tags.ReplaceWithText(Gallery->GetTagsString(), ";");
            }
                    
            const auto result = DualView::Get().DualView::AddToCollection(DownloadedImages,
                true, Gallery->GetTargetGalleryName(), tags, [&](float progress)
                {
                    Loader->_SetDLThreadStatus("Importing Gallery: " +
                        Gallery->GetTargetGalleryName(), true, progress);
                });

            LEVIATHAN_ASSERT(result, "Downloader's import failed");

            LOG_INFO("Downloader: imported " + Convert::ToString(DownloadedImages.size()) +
                " images to '" + Gallery->GetTargetGalleryName() + "'");

            // Add to folder //
            VirtualPath path(Gallery->GetTargetPath());

            if(!path.IsRootPath() && !Gallery->GetTargetGalleryName().empty() &&
                Gallery->GetTargetGalleryName() !=
                DualView::Get().GetUncategorized()->GetName())
            {
                DualView::Get().AddCollectionToFolder(
                    DualView::Get().GetFolderFromPath(path),
                    DualView::Get().GetDatabase().SelectCollectionByNameAG(
                        Gallery->GetTargetGalleryName()));

                LOG_INFO("Downloader: moved target collection '" +
                    Gallery->GetTargetGalleryName() + "' to: " +
                    static_cast<std::string>(path));
            }
                        
            Loader->_SetDLThreadStatus("Finished Downloading: " +
                Gallery->GetTargetGalleryName(), false, 1.0f);
            
            return true;
        }
        }

        return false;
    }

    Downloader* Loader;
    const std::shared_ptr<NetGallery> Gallery;
    const std::shared_ptr<DLListItem> Widget;


    STATE state = STATE::INITIAL;

    std::atomic<bool> ImageListReady = { false };
    std::vector<std::shared_ptr<NetFile>> ImageList;
    size_t CurrentDownload = 0;


    //! Download retries used
    int DLRetries = 0;

    std::vector<std::shared_ptr<Image>> DownloadedImages;

    std::shared_ptr<ImageFileDLJob> imagedl;
    
};

void Downloader::_RunDownloadThread(){

    std::unique_lock<std::mutex> lock(DownloadThreadMutex);

    std::shared_ptr<DownloadProgressState> dlState;

    while(RunDownloadThread){

        if(dlState){

            if(dlState->Tick(dlState)){

                _DLFinished(dlState->Widget);
                dlState.reset();
            }
            
        } else {

            auto gallery = GetNextSelectedGallery();

            if(gallery){

                dlState = std::make_shared<DownloadProgressState>(this, gallery,
                    gallery->GetGallery());
            }
        }
        
        NotifyDownloadThread.wait_for(lock, std::chrono::milliseconds(10));
    }

    _SetDLThreadStatus("Downloader Stopped", false, 1.0f);
}


// ------------------------------------ //
void Downloader::_ToggleDownloadThread(){

    if(RunDownloadThread){
        
        StopDownloadThread();
        DLStatusLabel->set_text("Not downloading");
        
    } else {

        StartDownloadThread();
        DLStatusLabel->set_text("Downloader thread waiting for work");
    }
}
// ------------------------------------ //


