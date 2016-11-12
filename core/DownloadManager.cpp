// ------------------------------------ //
#include "DownloadManager.h"

#include "CurlWrapper.h"

#include "core/DualView.h"
#include "core/Settings.h"
#include "Common.h"

using namespace DV;
// ------------------------------------ //

DownloadManager::DownloadManager(){

    DownloadThread = std::thread(&DownloadManager::_RunDLThread, this);
    
}

DownloadManager::~DownloadManager(){

    // Makes sure the thread is marked as closing
    StopDownloads();

    // Notify it just in case it's waiting
    NotifyThread.notify_all();
    DownloadThread.join();

    if(!WorkQueue.empty()){

        LOG_WARNING("DownloadManager quit with items still waiting to be downloaded");
        
    } else {

        LOG_INFO("DownloadManager exited cleanly");
    }
}
// ------------------------------------ //
void DownloadManager::StopDownloads(){

    ThreadQuit = true;
    NotifyThread.notify_all();
}
// ------------------------------------ //
void DownloadManager::_RunDLThread(){

    std::unique_lock<std::mutex> lock(WorkQueueMutex);

    while(!ThreadQuit){

        // Wait for work //
        if(WorkQueue.empty())
            NotifyThread.wait(lock);

        if(WorkQueue.empty())
            continue;

        // Handle the download objects
        std::shared_ptr<DownloadJob> item = WorkQueue.front();
        WorkQueue.pop_front();

        // Unlock while working on an item
        lock.unlock();

        item->DoDownload(*this);
        item.reset();

        lock.lock();
    }

    LOG_INFO("Download Thread Quit");
}
// ------------------------------------ //
void DownloadManager::QueueDownload(std::shared_ptr<DownloadJob> job){

    std::unique_lock<std::mutex> lock(WorkQueueMutex);
    WorkQueue.push_back(job);
    
    NotifyThread.notify_all();
}
// ------------------------------------ //
// DownloadJob
DownloadJob::DownloadJob(const std::string &url, const std::string &referrer) :
    URL(url), Referrer(referrer)
{

}

void DownloadJob::DoDownload(DownloadManager &manager){

    LOG_INFO("DownloadJob running: " + URL);

    CurlWrapper curl;

    bool debug = DualView::Get().GetSettings().GetCurlDebug();

    if(debug){

        LOG_INFO("Downloads using curl debug");
    }
    


    // Download finished successfully
    HandleContent();
}
// ------------------------------------ //
// DownloadJob
PageScanJob::PageScanJob(const std::string &url, const std::string &referrer /*= ""*/) :
    DownloadJob(url, referrer)
{

}
    
void PageScanJob::HandleContent(){

    LOG_INFO("PageScanJob scanning links");
}
