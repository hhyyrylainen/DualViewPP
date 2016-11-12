#pragma once

#include <thread>
#include <list>
#include <vector>

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace DV{

class DownloadManager;

size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata);

//! \brief A job for the downloader to do
class DownloadJob{
    friend size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
public:

    DownloadJob(const std::string &url, const std::string &referrer);

    //! \brief Called on the download thread to process this download
    virtual void DoDownload(DownloadManager &manager);

    //! \brief Called from curl when the download has progressed
    //! \returns True if download should be canceled
    virtual bool OnDownloadProgress(float dlprogress, float uploadprogress);
    
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

//! \brief Downloads a file to a local file in the staging folder
class ImageFileDLJob : public DownloadJob{
public:

    //! \param replacelocal If true the local filename is not made unique before downloading.
    //! if false numbers are added to the end of the name if it exists already
    ImageFileDLJob(const std::string &url, const std::string &referrer,
        bool replacelocal = false);
    

protected:

    //! Once download has started (or finished) this contains the local file path
    std::string LocalFile;
    
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

    //! \brief Extracts a filename from an url
    static std::string ExtractFileName(const std::string &url);

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
