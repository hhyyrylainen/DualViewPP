#pragma once

#include "Plugin.h"

#include <list>
#include <thread>
#include <vector>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>

namespace DV {

constexpr auto PAGE_SCAN_RETRIES = 5;

class DownloadManager;

size_t CurlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata);

//! \brief A job for the downloader to do
class DownloadJob {
    friend size_t CurlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata);

public:
    DownloadJob(const std::string& url, const std::string& referrer);

    //! \brief Called on the download thread to process this download
    virtual void DoDownload(DownloadManager& manager);

    //! \brief Called from curl when the download has progressed
    //! \returns True if download should be canceled
    //! \todo Actually implement timeout
    virtual bool OnDownloadProgress(float dlprogress, float uploadprogress);

    //! \brief Sets a finish callback for this job
    void SetFinishCallback(const std::function<void(DownloadJob&, bool)>& callback);

    const std::string& GetDownloadedBytes() const
    {

        return DownloadBytes;
    }

    //! Resets the state to allow retrying this download
    void Retry()
    {

        DownloadBytes.clear();
        HasFinished = false;
        HasSucceeded = true;
        Progress = 0;
    }

    bool IsReady() const
    {

        return HasFinished;
    }

    bool HasFailed() const
    {

        return !HasSucceeded;
    }

    auto GetURL() const
    {

        return URL;
    }

    //! \brief Externally set this as failed
    void SetAsFailed()
    {

        HasSucceeded = false;
    }

protected:
    virtual void HandleContent() = 0;
    virtual void HandleError()
    {
        OnFinished(false);
    };

    //! \brief Called from HandleContent or HandleError once finished
    virtual void OnFinished(bool success);

protected:
    std::string URL;
    std::string Referrer;

    //! Holds data while downloading
    std::string DownloadBytes;

    //! After fetching has the content type if the server sent the type to us
    std::string DownloadedContentType;

    bool HasFinished = false;
    bool HasSucceeded = true;

    //! Current progress. Range 0.0f - 1.0f
    std::atomic<float> Progress;

    std::function<void(DownloadJob&, bool)> FinishCallback;
};

//! \brief Scans a single page and gets a list of all the links and content on it
//! with the help of a plugin that can handle this website
class PageScanJob : public DownloadJob {
public:
    //! \exception Leviathan::InvalidArgument if the URL is not supported
    //! \param initialpage True if this is the main page and tag scanning should be forced
    //! on even if the scanner for the url doesn't usually automatically find tags
    PageScanJob(const std::string& url, bool initialpage, const std::string& referrer = "");

    ScanResult& GetResult()
    {

        return Result;
    }

protected:
    void HandleContent() override;

protected:
    std::vector<std::string> Links;
    std::vector<std::string> Content;

    bool InitialPage = false;
    ScanResult Result;
};

//! \brief Downloads a file to a local file in the staging folder
class ImageFileDLJob : public DownloadJob {
public:
    //! \param replacelocal If true the local filename is not made unique before downloading.
    //! if false numbers are added to the end of the name if it exists already
    ImageFileDLJob(
        const std::string& url, const std::string& referrer, bool replacelocal = false);

    auto GetLocalFile() const
    {

        return LocalFile;
    }

protected:
    //! Writes the received data to a file
    void HandleContent() override;

protected:
    //! Once download has started (or finished) this contains the local file path
    std::string LocalFile;

    bool ReplaceLocal;
};

//! \brief A fake download that loads a local file
class LocallyCachedDLJob : public DownloadJob {
public:
    //! \exception Leviathan::InvalidArgument if file doesn't exist
    LocallyCachedDLJob(const std::string& file);

    void DoDownload(DownloadManager& manager) override;

protected:
    void HandleContent() override;
};

//! \brief A basic download that saves response to memory
class MemoryDLJob : public DownloadJob {
public:
    MemoryDLJob(const std::string& url, const std::string& referrer);

protected:
    void HandleContent() override;
};



//! \brief Handles scanning pages for content and downloading found content
//!
//! Uses plugins to handle contents of webpages once downloaded
class DownloadManager {
public:
    DownloadManager();
    ~DownloadManager();

    //! \brief Makes the download thread quit after it has processed the current download
    void StopDownloads();

    //! \brief Adds an item to the work queue
    void QueueDownload(std::shared_ptr<DownloadJob> job);

    //! \brief Extracts a filename from an url
    static std::string ExtractFileName(const std::string& url);

    //! \brief Returns a local path for caching an URL
    static std::string GetCachePathForURL(const std::string& url);

protected:
    //! Main function for DownloadThread
    void _RunDLThread();

protected:
    std::thread DownloadThread;

    std::atomic<bool> ThreadQuit = {false};

    std::condition_variable NotifyThread;

    std::mutex WorkQueueMutex;
    std::list<std::shared_ptr<DownloadJob>> WorkQueue;
};


} // namespace DV
