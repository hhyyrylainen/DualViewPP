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
std::string DownloadManager::ExtractFileName(const std::string &url){

    auto lastslash = url.find_last_of('/');

    if(lastslash != std::string::npos)
        return ExtractFileName(url.substr(lastslash + 1));

    // Get until a ? or #
    size_t length = 0;

    for(size_t i = 0; i < url.size(); ++i){

        if(url[i] == '?' || url[i] == '#')
            break;
        length = i;
    }

    return url.substr(0, length + 1);
}
// ------------------------------------ //
// DownloadJob
DownloadJob::DownloadJob(const std::string &url, const std::string &referrer) :
    URL(url), Referrer(referrer)
{

}

int CurlProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow)
{
    auto* obj = reinterpret_cast<DownloadJob*>(clientp);

    return obj->OnDownloadProgress(
        dltotal != 0 ? static_cast<float>(dlnow) / dltotal : 0.f,
        ultotal != 0 ? static_cast<float>(ulnow) / ultotal : 0.f);
}

bool DownloadJob::OnDownloadProgress(float dlprogress, float uploadprogress){

    // Timeout //
    if(false){

        LOG_WARNING("DownloadJob: timing out: " + URL);
        return true;
    }

    //LOG_WRITE("DL progress: " + Convert::ToString(dlprogress));

    // Continue //
    return false;
}

size_t DV::CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata){

    auto* obj = reinterpret_cast<DownloadJob*>(userdata);

    obj->DownloadBytes.append(ptr, size * nmemb);
    return size * nmemb;
}

void DownloadJob::DoDownload(DownloadManager &manager){

    LOG_INFO("DownloadJob running: " + URL);

    // Holds curl errors //
    char curlError[CURL_ERROR_SIZE];

    CurlWrapper curlwrapper;
    CURL* curl = curlwrapper.Get();

    bool debug = DualView::Get().GetSettings().GetCurlDebug();

    if(debug){

        LOG_INFO("Downloads using curl debug");
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); 
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());

    if(!Referrer.empty()){

        curl_easy_setopt(curl, CURLOPT_REFERER, Referrer.c_str());
    }

    // Mozilla useragent
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (X11; Fedora; Linux x86_64; rv:49.0) Gecko/20100101 Firefox/49.0");

    // Capture error messages
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError); 

    // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    // Max 10 redirects
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10);

    // Data retrieval //
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

    // Progress callback //
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &CurlProgressCallback);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);

    // Do download
    const auto result = curl_easy_perform(curl);

    if(result == CURLE_OK){
        
        // Download finished successfully
        HandleContent();
        return;
    }

    // Handle error //
    curlError[CURL_ERROR_SIZE - 1] = '\0';

    std::string error = curlError;



    LOG_ERROR("Curl failed with error(" + Convert::ToString(result) + "): " + error);

    
}
// ------------------------------------ //
// DownloadJob
PageScanJob::PageScanJob(const std::string &url, const std::string &referrer /*= ""*/) :
    DownloadJob(url, referrer)
{

}
    
void PageScanJob::HandleContent(){

    LOG_INFO("PageScanJob scanning links");
    //LOG_WRITE("Data: " + DownloadBytes);
}
