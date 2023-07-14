#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

#include "ProcessableURL.h"
#include "ScanResult.h"
#include "TaskListWithPriority.h"

namespace DV
{
constexpr auto PAGE_SCAN_RETRIES = 6;

constexpr auto DOWNLOADER_USER_AGENT = "Mozilla/5.0 (X11; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/114.0";

class DownloadManager;

size_t CurlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata);

//! \brief A job for the downloader to do
class DownloadJob
{
    friend size_t CurlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata);

public:
    explicit DownloadJob(const ProcessableURL& url);
    virtual ~DownloadJob() = default;

    //! \brief Called on the download thread to process this download
    virtual void DoDownload(DownloadManager& manager);

    //! \brief Called from curl when the download has progressed
    //! \returns True if download should be canceled
    //! \todo Actually implement timeout
    virtual bool OnDownloadProgress(float dlprogress, float uploadprogress);

    //! \brief Sets a finish callback for this job
    //! \param callback Called on finish, gets passed the data and success flag. If returns false this forces a retry
    //! \param once If true the callback is only called once and then cleared
    void SetFinishCallback(const std::function<bool(DownloadJob&, bool)>& callback, bool once = true);

    [[nodiscard]] const std::string& GetDownloadedBytes() const
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

    [[nodiscard]] bool IsReady() const
    {
        return HasFinished;
    }

    [[nodiscard]] bool HasFailed() const
    {
        return !HasSucceeded;
    }

    [[nodiscard]] auto GetURL() const
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
    }

    //! \brief Called from HandleContent or HandleError once finished
    virtual void OnFinished(bool success);

protected:
    ProcessableURL URL;

    //! Holds data while downloading
    std::string DownloadBytes;

    //! Contains the content type after fetching has the content type if the server sent the type to us
    std::string DownloadedContentType;

    bool HasFinished = false;
    bool HasSucceeded = true;

    //! Current progress. Range 0.0f - 1.0f
    std::atomic<float> Progress;

    std::function<bool(DownloadJob&, bool)> FinishCallback;
    bool FinishCallbackIsRanOnce = true;
};

//! \brief Scans a single page and gets a list of all the links and content on it
//! with the help of a plugin that can handle this website
class PageScanJob : public DownloadJob
{
public:
    //! \exception Leviathan::InvalidArgument if the URL is not supported
    //! \param initialpage True if this is the main page and tag scanning should be forced
    //! on even if the scanner for the url doesn't usually automatically find tags
    PageScanJob(const ProcessableURL& url, bool initialpage);

    ScanResult& GetResult()
    {
        return Result;
    }

protected:
    void HandleContent() override;

protected:
    std::vector<ProcessableURL> Links;
    std::vector<ProcessableURL> Content;

    bool InitialPage = false;
    ScanResult Result;
};

//! \brief Downloads a file to a local file in the staging folder
class ImageFileDLJob : public DownloadJob
{
public:
    //! \param replaceLocal If true the local filename is not made unique before downloading.
    //! if false numbers are added to the end of the name if it exists already
    explicit ImageFileDLJob(const ProcessableURL& url, bool replaceLocal = false);

    [[nodiscard]] auto GetLocalFile() const
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
class LocallyCachedDLJob : public DownloadJob
{
public:
    //! \exception Leviathan::InvalidArgument if file doesn't exist
    explicit LocallyCachedDLJob(const std::string& file);

    void DoDownload(DownloadManager& manager) override;

protected:
    void HandleContent() override;
};

//! \brief A basic download that saves response to memory
class MemoryDLJob : public DownloadJob
{
public:
    explicit MemoryDLJob(const ProcessableURL& url);

protected:
    void HandleContent() override;
};

//! \brief Handles scanning pages for content and downloading found content
//!
//! Uses plugins to handle contents of webpages once downloaded
class DownloadManager
{
public:
    DownloadManager();
    ~DownloadManager();

    //! \brief Makes the download thread quit after it has processed the current download
    void StopDownloads();

    //! \brief Adds an item to the work queue
    std::shared_ptr<BaseTaskItem> QueueDownload(std::shared_ptr<DownloadJob> job, int64_t priority = -1);

    //! \brief Extracts a filename from an url
    [[nodiscard]] static std::string ExtractFileName(const std::string& url);

    [[nodiscard]] static inline std::string ExtractFileName(const ProcessableURL& url)
    {
        return ExtractFileName(url.GetURL());
    }

    //! \brief Returns a local path for caching an URL
    [[nodiscard]] static std::string GetCachePathForURL(const std::string& url);

    [[nodiscard]] static std::string GetCachePathForURL(const ProcessableURL& url)
    {
        return GetCachePathForURL(url.GetURL());
    }

protected:
    //! Main function for DownloadThread
    void RunDLThread();

protected:
    std::thread DownloadThread;

    std::atomic<bool> ThreadQuit{false};

    std::condition_variable NotifyThread;

    TaskListWithPriority<std::shared_ptr<DownloadJob>> WorkQueue;
};

} // namespace DV
