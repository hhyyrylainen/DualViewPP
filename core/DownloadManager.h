#pragma once

#include <thread>
#include <list>
#include <vector>

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace DV{

class DownloadManager;

//! \brief A job for the downloader to do
class DownloadJob{
public:

    DownloadJob(const std::string &url, const std::string &referrer);

    //! \brief Called on the download thread to process this download
    virtual void DoDownload(DownloadManager &manager);
    
protected:

    virtual void HandleContent() = 0;
    virtual void HandleError(){};
    
protected:

    std::string URL;
    std::string Referrer;

    //! Holds data while downloading
    std::string DownloadBytes;

    //! Current progress. Range 0.0f - 1.0f
    std::atomic<float> Progress;
};

//! \brief Scans a single page and gets a list of all the links and content on it
//! with the help of a plugin that can handle this website
class PageScanJob : public DownloadJob{
public:

    PageScanJob(const std::string &url, const std::string &referrer = "");
    
protected:

    void HandleContent() override;

protected:
    
    std::vector<std::string> Links;
    std::vector<std::string> Content;
};

//! \brief Handles scanning pages for content and downloading found content
//!
//! Uses plugins to handle contents of webpages once downloaded
class DownloadManager{
public:

    DownloadManager();
    ~DownloadManager();

    //! \brief Makes the download thread quit after it has processed the current download
    void StopDownloads();

    //! \brief Adds an item to the work queue
    void QueueDownload(std::shared_ptr<DownloadJob> job);
    

protected:

    //! Main function for DownloadThread
    void _RunDLThread();

protected:

    std::thread DownloadThread;

    std::atomic<bool> ThreadQuit = { false };

    std::condition_variable NotifyThread;

    std::mutex WorkQueueMutex;
    std::list<std::shared_ptr<DownloadJob>> WorkQueue;
};


}
