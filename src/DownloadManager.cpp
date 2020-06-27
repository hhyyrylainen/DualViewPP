// ------------------------------------ //
#include "DownloadManager.h"

#include "CurlWrapper.h"

#include "DualView.h"
#include "PluginManager.h"
#include "Settings.h"

#include "Common.h"

#include "Common/StringOperations.h"
#include "FileSystem.h"

#include <boost/filesystem.hpp>

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

    if(!WorkQueue.empty()) {

        LOG_WARNING("DownloadManager quit with items still waiting to be downloaded");

    } else {

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
    std::unique_lock<std::mutex> lock(WorkQueueMutex);

    while(!ThreadQuit) {

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
void DownloadManager::QueueDownload(std::shared_ptr<DownloadJob> job)
{
    std::unique_lock<std::mutex> lock(WorkQueueMutex);
    WorkQueue.push_back(job);

    NotifyThread.notify_all();
}
// ------------------------------------ //
std::string DownloadManager::ExtractFileName(const std::string& url)
{
    auto lastslash = url.find_last_of('/');

    if(lastslash != std::string::npos)
        return ExtractFileName(url.substr(lastslash + 1));

    // Get until a ? or #
    size_t length = 0;

    for(size_t i = 0; i < url.size(); ++i) {

        if(url[i] == '?' || url[i] == '#')
            break;
        length = i;
    }

    auto name = url.substr(0, length + 1);

    // Unescape things like spaces
    {
        CurlWrapper curl;
        int outlength;

        auto unescaped =
            curl_easy_unescape(curl.Get(), name.c_str(), name.length(), &outlength);

        name = std::string(unescaped, outlength);

        curl_free(unescaped);
    }

    // Remove unwanted characters like /'s
    // (Looks like curl refuses to decode %2 (/) so it might be safe without this)
    return Leviathan::StringOperations::ReplaceSingleCharacter<std::string>(name, "/\\", '_');
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
class RetryDownload : std::exception {};

// ------------------------------------ //
// DownloadJob
DownloadJob::DownloadJob(const std::string& url, const std::string& referrer) :
    URL(url), Referrer(referrer)
{}

void DownloadJob::SetFinishCallback(const std::function<void(DownloadJob&, bool)>& callback)
{
    FinishCallback = callback;
}

void DownloadJob::OnFinished(bool success)
{
    HasFinished = true;
    HasSucceeded = success;

    if(FinishCallback)
        FinishCallback(*this, success);
}

int CurlProgressCallback(
    void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    auto* obj = reinterpret_cast<DownloadJob*>(clientp);

    return obj->OnDownloadProgress(dltotal != 0 ? static_cast<float>(dlnow) / dltotal : 0.f,
        ultotal != 0 ? static_cast<float>(ulnow) / ultotal : 0.f);
}

bool DownloadJob::OnDownloadProgress(float dlprogress, float uploadprogress)
{
    // Timeout //
    if(false) {

        LOG_WARNING("DownloadJob: timing out: " + URL);
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
    LOG_INFO("DownloadJob running: " + URL);

    // Holds curl errors //
    char curlError[CURL_ERROR_SIZE];

    CurlWrapper curlwrapper;
    CURL* curl = curlwrapper.Get();

    bool debug = DualView::Get().GetSettings().GetCurlDebug();

    if(debug) {

        LOG_INFO("Downloads using curl debug");
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
    }

    // Escape the url in case it has spaces or other things
    {
        const auto urlBase = Leviathan::StringOperations::BaseHostName(URL);
        auto path = Leviathan::StringOperations::URLPath(URL, false);

        auto questionMark = path.find_first_of('?');
        auto hashMark = path.find_first_of('#');

        std::string queryPart;

        if(hashMark != std::string::npos &&
            (questionMark == std::string::npos || hashMark < questionMark)) {
            queryPart = path.substr(hashMark, path.size() - hashMark);
            path = path.substr(0, hashMark);
        } else if(questionMark != std::string::npos) {

            queryPart = path.substr(questionMark, path.size() - questionMark);
            path = path.substr(0, questionMark);
        }

        // Looks like we first have to unescape and then escape
        int outlength;
        auto fullyUnEscaped =
            curl_easy_unescape(curl, path.c_str(), path.length(), &outlength);

        path = std::string(fullyUnEscaped, outlength);

        curl_free(fullyUnEscaped);
        fullyUnEscaped = nullptr;

        auto escaped = curl_easy_escape(curl, path.c_str(), path.size());

        if(!escaped) {

            LOG_ERROR("Escaping url failed: " + path);
            HandleError();
            return;
        }

        // Except we don't want to escape '/'s
        const auto partiallyEscaped = Leviathan::StringOperations::Replace<std::string>(
            std::string(escaped), "%2F", "/");

        URL = Leviathan::StringOperations::CombineURL(urlBase, partiallyEscaped);
        URL += queryPart;

        curl_free(escaped);
    }

    LOG_INFO("DownloadJob: Escaped download url is: " + URL);
    curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());

    if(!Referrer.empty()) {

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

    // Max time of 2 minutes
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60 * 2);

    // Data retrieval //
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

    // Progress callback //
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &CurlProgressCallback);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);

    // Do download
    // TODO: replace the retries and sleeps here to place new download entries into the
    // dl queue
    for(int i = 0; i < PAGE_SCAN_RETRIES; ++i) {
        const auto result = curl_easy_perform(curl);

        if(result == CURLE_OK) {

            // Download finished successfully

            // Check HTTP result code
            long httpcode = 0;

            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);

            if(httpcode != 200) {

                LOG_ERROR("received HTTP error code: " + Convert::ToString(httpcode) +
                          " from url " + URL);
                LOG_WRITE("Response data: " + DownloadBytes);

                if(httpcode == 429) {

                    int sleepTime = 2 + (i * 5);

                    LOG_WARNING("Got slow down status code (429). "
                                "Waiting " +
                                std::to_string(sleepTime) + " seconds before retry");
                    std::this_thread::sleep_for(std::chrono::seconds(sleepTime));
                }

            dlretrylabel:
                LOG_INFO("Retrying url download: " + URL);
                Retry();
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                continue;
            }

            // Get content type
            char* contentptr = nullptr;

            if(CURLE_OK == curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &contentptr) &&
                contentptr) {
                DownloadedContentType = std::string(contentptr);
            }

            try {
                HandleContent();
            } catch(const RetryDownload&) {

                goto dlretrylabel;
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

    LOG_ERROR("URL download ran out of retries: " + URL);
    HandleError();
}
// ------------------------------------ //
// PageScanJob
PageScanJob::PageScanJob(
    const std::string& url, bool initialpage, const std::string& referrer /*= ""*/) :
    DownloadJob(url, referrer),
    InitialPage(initialpage)
{
    auto scanner = DualView::Get().GetPluginManager().GetScannerForURL(url);

    if(!scanner)
        throw Leviathan::InvalidArgument("Unsupported website for url");
}

void PageScanJob::HandleContent()
{
    auto scanner = DualView::Get().GetPluginManager().GetScannerForURL(URL);

    if(!scanner) {

        LOG_ERROR("PageScanJob: scanner is not found anymore with url: " + URL);
        HandleError();
        return;
    }

    LOG_INFO("PageScanJob scanning links with: " + std::string(scanner->GetName()));


    Result = scanner->ScanSite({DownloadBytes, URL, DownloadedContentType, InitialPage});

    if(Result.ContentLinks.empty() && scanner->ScanAgainIfNoImages(URL)) {

        LOG_INFO("PageScanJob: running again because found no content and scanner "
                 "has ScanAgainIfNoImages = true");
    }

    // Copy result //
    Result.PrintInfo();

    OnFinished(true);
}
// ------------------------------------ //
// ImageFileDLJob
ImageFileDLJob::ImageFileDLJob(
    const std::string& url, const std::string& referrer, bool replacelocal /*= false*/) :
    DownloadJob(url, referrer),
    ReplaceLocal(replacelocal)
{}

void ImageFileDLJob::HandleContent()
{
    LOG_WRITE("TODO: check the file integrity as an image before succeeding");

    // Generate filename //
    if(ReplaceLocal) {

        LocalFile =
            (boost::filesystem::path(DualView::Get().GetSettings().GetStagingFolder()) /
                DownloadManager::ExtractFileName(URL))
                .string();

    } else {
        LocalFile = DualView::MakePathUniqueAndShort(
            (boost::filesystem::path(DualView::Get().GetSettings().GetStagingFolder()) /
                DownloadManager::ExtractFileName(URL))
                .string());
    }

    LOG_INFO("Writing downloaded image to file: " + LocalFile);

    Leviathan::FileSystem::WriteToFile(DownloadBytes, LocalFile);

    OnFinished(true);
}
// ------------------------------------ //
// LocallyCachedDLJob
LocallyCachedDLJob::LocallyCachedDLJob(const std::string& file) : DownloadJob(file, "")
{
    if(!boost::filesystem::exists(file)) {

        throw Leviathan::InvalidArgument("LocallyCachedDLJob: file doesn't exist");
    }
}

void LocallyCachedDLJob::DoDownload(DownloadManager& manager)
{
    Leviathan::FileSystem::ReadFileEntirely(URL, DownloadBytes);

    OnFinished(true);
}

void LocallyCachedDLJob::HandleContent()
{
    OnFinished(true);
}
// ------------------------------------ //
// MemoryDLJob
MemoryDLJob::MemoryDLJob(const std::string& url, const std::string& referrer) :
    DownloadJob(url, referrer)
{}

void MemoryDLJob::HandleContent()
{
    OnFinished(true);
}
