// ------------------------------------ //
#include "DownloadManager.h"

#include <boost/filesystem.hpp>

#include "Common/StringOperations.h"

#include "Common.h"
#include "CurlWrapper.h"
#include "DualView.h"
#include "FileSystem.h"
#include "PluginManager.h"
#include "Settings.h"
#include "TimeHelpers.h"

using namespace DV;

// ------------------------------------ //
DownloadManager::DownloadManager()
{
    DownloadThread = std::thread(&DownloadManager::_RunDLThread, this);
}

DownloadManager::~DownloadManager()
{
    // Makes sure the thread is marked as closing
    StopDownloads();

    // Notify it just in case it's waiting
    NotifyThread.notify_all();
    DownloadThread.join();

    GUARD_LOCK_OTHER(WorkQueue);
    if (!WorkQueue.Empty(guard))
    {
        LOG_WARNING("DownloadManager quit with items still waiting to be downloaded");
    }
    else
    {
        LOG_INFO("DownloadManager exited cleanly");
    }
}

// ------------------------------------ //
void DownloadManager::StopDownloads()
{
    ThreadQuit = true;
    NotifyThread.notify_all();
}

// ------------------------------------ //
void DownloadManager::_RunDLThread()
{
    GUARD_LOCK_OTHER(WorkQueue);

    while (!ThreadQuit)
    {
        // Wait for work //
        if (WorkQueue.Empty(guard))
            NotifyThread.wait(guard);

        auto task = WorkQueue.Pop(guard);

        if (!task)
            continue;

        // Handle the download objects

        // Unlock while working on an item
        guard.unlock();

        task->Task->DoDownload(*this);
        // Needed? the tasks are now const
        // task->Task.reset();
        task->OnDone();

        guard.lock();
    }

    LOG_INFO("Download Thread Quit");
}

// ------------------------------------ //
std::shared_ptr<BaseTaskItem> DownloadManager::QueueDownload(
    std::shared_ptr<DownloadJob> job, int64_t priority /*= -1*/)
{
    if (priority == -1)
        priority = TimeHelpers::GetCurrentUnixTimestamp();

    GUARD_LOCK_OTHER(WorkQueue);

    auto task = WorkQueue.Push(guard, job, priority);

    NotifyThread.notify_all();
    return task;
}

// ------------------------------------ //
std::string DownloadManager::ExtractFileName(const std::string& url)
{
    auto lastslash = url.find_last_of('/');

    if (lastslash != std::string::npos)
        return ExtractFileName(url.substr(lastslash + 1));

    // Get until a ? or #
    size_t length = 0;

    for (size_t i = 0; i < url.size(); ++i)
    {
        if (url[i] == '?' || url[i] == '#')
            break;
        length = i;
    }

    auto name = url.substr(0, length + 1);

    // Unescape things like spaces
    {
        CurlWrapper curl;
        int outlength;

        auto unescaped = curl_easy_unescape(curl.Get(), name.c_str(), name.length(), &outlength);

        name = std::string(unescaped, outlength);

        curl_free(unescaped);
    }

    // Remove unwanted characters like /'s
    // (Looks like curl refuses to decode %2 (/) so it might be safe without this)
    return Leviathan::StringOperations::ReplaceSingleCharacter<std::string>(
        Leviathan::StringOperations::ReplaceSingleCharacter<std::string>(name, '/', '_'), '\\', '_');
}

std::string DownloadManager::GetCachePathForURL(const std::string& url)
{
    return (boost::filesystem::path(DualView::Get().GetSettings().GetStagingFolder()) /
        (DualView::CalculateBase64EncodedHash(url) + "." +
            Leviathan::StringOperations::GetExtension(ExtractFileName(url))))
        .string();
}

// ------------------------------------ //
// Run again exception for DownloadJob
class RetryDownload : std::exception
{
};

// ------------------------------------ //
// DownloadJob
DownloadJob::DownloadJob(const ProcessableURL& url) : URL(url)
{
}

void DownloadJob::SetFinishCallback(const std::function<bool(DownloadJob&, bool)>& callback)
{
    FinishCallback = callback;
}

void DownloadJob::OnFinished(bool success)
{
    HasFinished = true;
    HasSucceeded = success;

    if (FinishCallback)
    {
        if (!FinishCallback(*this, success))
        {
            LOG_INFO("Retrying url download as finish callback signaled a problem with the data to request a retry: " +
                URL.GetURL());
            throw RetryDownload();
        }
    }
}

int CurlProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    auto* obj = reinterpret_cast<DownloadJob*>(clientp);

    return obj->OnDownloadProgress(dltotal != 0 ? static_cast<float>(dlnow) / static_cast<float>(dltotal) : 0.f,
        ultotal != 0 ? static_cast<float>(ulnow) / static_cast<float>(ultotal) : 0.f);
}

bool DownloadJob::OnDownloadProgress(float dlprogress, float uploadprogress)
{
    // TODO: add timeout
    // Timeout //
    if (false)
    {
        LOG_WARNING("DownloadJob: timing out: " + URL.GetURL());
        return true;
    }

    // LOG_WRITE("DL progress: " + Convert::ToString(dlprogress));
    Progress = std::max(dlprogress, uploadprogress);

    // Continue //
    return false;
}

size_t DV::CurlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* obj = reinterpret_cast<DownloadJob*>(userdata);

    obj->DownloadBytes.append(ptr, size * nmemb);
    return size * nmemb;
}

void DownloadJob::DoDownload(DownloadManager& manager)
{
    if (URL.HasCanonicalURL())
    {
        LOG_INFO("DownloadJob running: " + URL.GetURL() + " (canonical: " + URL.GetCanonicalURL() + ")");
    }
    else
    {
        LOG_INFO("DownloadJob running: " + URL.GetURL());
    }

    // Holds curl errors //
    char curlError[CURL_ERROR_SIZE];

    CurlWrapper curlWrapper;
    CURL* curl = curlWrapper.Get();

    bool debug = DualView::Get().GetSettings().GetCurlDebug();

    if (debug)
    {
        LOG_INFO("Downloads using curl debug");
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
    }

    auto finalURL = URL.GetURL();

    // Escape the url in case it has spaces or other things
    {
        const auto urlBase = Leviathan::StringOperations::BaseHostName(finalURL);
        auto path = Leviathan::StringOperations::URLPath(finalURL, false);

        auto questionMark = path.find_first_of('?');
        auto hashMark = path.find_first_of('#');

        std::string queryPart;

        if (hashMark != std::string::npos && (questionMark == std::string::npos || hashMark < questionMark))
        {
            queryPart = path.substr(hashMark, path.size() - hashMark);
            path = path.substr(0, hashMark);
        }
        else if (questionMark != std::string::npos)
        {
            queryPart = path.substr(questionMark, path.size() - questionMark);
            path = path.substr(0, questionMark);
        }

        // Looks like we first have to unescape and then escape
        int outlength;
        auto fullyUnEscaped = curl_easy_unescape(curl, path.c_str(), static_cast<int>(path.length()), &outlength);

        path = std::string(fullyUnEscaped, outlength);

        curl_free(fullyUnEscaped);
        fullyUnEscaped = nullptr;

        auto escaped = curl_easy_escape(curl, path.c_str(), static_cast<int>(path.length()));

        if (!escaped)
        {
            LOG_ERROR("Escaping url failed: " + path);
            HandleError();
            return;
        }

        // Except we don't want to escape '/'s
        const auto partiallyEscaped =
            Leviathan::StringOperations::Replace<std::string>(std::string(escaped), "%2F", "/");

        finalURL = Leviathan::StringOperations::CombineURL(urlBase, partiallyEscaped);
        finalURL += queryPart;

        curl_free(escaped);
    }

    LOG_INFO("DownloadJob: Escaped download url is: " + finalURL);
    curl_easy_setopt(curl, CURLOPT_URL, finalURL.c_str());

    if (URL.HasReferrer())
    {
        curl_easy_setopt(curl, CURLOPT_REFERER, URL.GetReferrer().c_str());
    }

    if (URL.HasCookies())
    {
        curl_easy_setopt(curl, CURLOPT_COOKIE, URL.GetCookies().c_str());
    }

    // Mozilla useragent
    curl_easy_setopt(curl, CURLOPT_USERAGENT, DOWNLOADER_USER_AGENT);

    // Capture error messages
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);

    // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    // Max 10 redirects
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10);

    // Max time of 120 minutes (to just avoid total lockups)
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60 * 120);

    // Timeout with connection
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15);

    // Timeout low speeds
    // Timespan of 15 seconds
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 15);
    // 20 kbytes per second
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 20000);

    // Data retrieval //
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

    // TODO: is CURLOPT_NOSIGNAL 1 required?

    // Progress callback //
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &CurlProgressCallback);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);

    // Do download
    // TODO: replace the retries and sleeps here to place new download entries into the
    // dl queue
    for (int i = 0; i < PAGE_SCAN_RETRIES; ++i)
    {
        const auto result = curl_easy_perform(curl);

        if (result == CURLE_OK)
        {
            // Download finished successfully

            // Check HTTP result code
            long httpCode = 0;

            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

            if (httpCode != 200)
            {
                LOG_ERROR("received HTTP error code: " + Convert::ToString(httpCode) + " from url " + finalURL);
                LOG_WRITE("Response data: " + DownloadBytes);

                if (httpCode == 429)
                {
                    int sleepTime = 2 + (i * 5);

                    LOG_WARNING("Got slow down status code (429). Waiting " + std::to_string(sleepTime) +
                        " seconds before retry");
                    std::this_thread::sleep_for(std::chrono::seconds(sleepTime));
                }

dlRetryLabel:
                LOG_INFO("Retrying url download: " + finalURL);
                Retry();
                std::this_thread::sleep_for(std::chrono::milliseconds(350 * static_cast<int>(std::pow(2, i + 1))));
                continue;
            }

            // Get content type
            char* contentPtr = nullptr;

            if (CURLE_OK == curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &contentPtr) && contentPtr)
            {
                DownloadedContentType = std::string(contentPtr);
            }

            try
            {
                HandleContent();
            }
            catch (const RetryDownload&)
            {
                goto dlRetryLabel;
            }

            return;
        }

        // Handle error //
        curlError[CURL_ERROR_SIZE - 1] = '\0';

        std::string error = curlError;

        LOG_ERROR("Curl failed with error(" + Convert::ToString(result) + "): " + error);
        HandleError();
        return;
    }

    LOG_ERROR("URL download ran out of retries: " + finalURL);
    HandleError();
}

// ------------------------------------ //
// PageScanJob
PageScanJob::PageScanJob(const ProcessableURL& url, bool initialpage) : DownloadJob(url), InitialPage(initialpage)
{
    auto scanner = DualView::Get().GetPluginManager().GetScannerForURL(url.GetURL());

    if (!scanner)
        throw Leviathan::InvalidArgument("Unsupported website for url");
}

void PageScanJob::HandleContent()
{
    auto scanner = DualView::Get().GetPluginManager().GetScannerForURL(URL.GetURL());

    if (!scanner)
    {
        LOG_ERROR("PageScanJob: scanner is not found anymore with url: " + URL.GetURL());
        HandleError();
        return;
    }

    LOG_INFO("PageScanJob scanning links with: " + std::string(scanner->GetName()));

    Result = scanner->ScanSite({DownloadBytes, URL, DownloadedContentType, InitialPage});

    if (Result.ContentLinks.empty() && scanner->ScanAgainIfNoImages(URL))
    {
        LOG_INFO("PageScanJob: running again because found no content and scanner has ScanAgainIfNoImages = true");

        throw RetryDownload();
    }

    // Show info in logs about the scan //
    Result.PrintInfo();

    OnFinished(true);
}

// ------------------------------------ //
// ImageFileDLJob
ImageFileDLJob::ImageFileDLJob(const ProcessableURL& url, bool replaceLocal /*= false*/) :
    DownloadJob(url), ReplaceLocal(replaceLocal)
{
}

void ImageFileDLJob::HandleContent()
{
    LOG_WRITE("TODO: check the file integrity as an image before succeeding");

    // Generate filename //
    if (ReplaceLocal)
    {
        LocalFile = (boost::filesystem::path(DualView::Get().GetSettings().GetStagingFolder()) /
            DownloadManager::ExtractFileName(URL.GetURL()))
                        .string();
    }
    else
    {
        LocalFile = DualView::MakePathUniqueAndShort(
            (boost::filesystem::path(DualView::Get().GetSettings().GetStagingFolder()) /
                DownloadManager::ExtractFileName(URL.GetURL()))
                .string());
    }

    LOG_INFO("Writing downloaded image to file: " + LocalFile);

    Leviathan::FileSystem::WriteToFile(DownloadBytes, LocalFile);

    OnFinished(true);
}

// ------------------------------------ //
// LocallyCachedDLJob
LocallyCachedDLJob::LocallyCachedDLJob(const std::string& file) : DownloadJob(ProcessableURL(file, true))
{
    if (!boost::filesystem::exists(file))
    {
        throw Leviathan::InvalidArgument("LocallyCachedDLJob: file doesn't exist");
    }
}

void LocallyCachedDLJob::DoDownload(DownloadManager& manager)
{
    Leviathan::FileSystem::ReadFileEntirely(URL.GetURL(), DownloadBytes);

    OnFinished(true);
}

void LocallyCachedDLJob::HandleContent()
{
    OnFinished(true);
}

// ------------------------------------ //
// MemoryDLJob
MemoryDLJob::MemoryDLJob(const ProcessableURL& url) : DownloadJob(url)
{
}

void MemoryDLJob::HandleContent()
{
    OnFinished(true);
}
