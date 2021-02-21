// ------------------------------------ //
#include "CacheManager.h"

#include "Common.h"
#include <Magick++.h>
#include <gtkmm.h>

#include "Exceptions.h"

#include "DualView.h"
#include "Settings.h"

#include "Common/StringOperations.h"

#include <boost/filesystem.hpp>

using namespace DV;
// ------------------------------------ //
CacheManager::CacheManager()
{
    Magick::InitializeMagick(Glib::get_current_dir().c_str());

    FullLoaderThread =
        std::thread(std::bind<void>(&CacheManager::_RunFullSizeLoaderThread, this));

    CacheCleanupThread =
        std::thread(std::bind<void>(&CacheManager::_RunCacheCleanupThread, this));

    ThumbnailGenerationThread =
        std::thread(std::bind<void>(&CacheManager::_RunThumbnailGenerationThread, this));
}

CacheManager::~CacheManager()
{
    FolderIconAsImage.reset();

    // Stop loading threads //
    // This might already have been set to true
    Quitting = true;

    NotifyFullLoaderThread.notify_all();
    NotifyCacheCleanup.notify_all();
    NotifyThumbnailGenerationThread.notify_all();

    // Wait for threads to quit //
    FullLoaderThread.join();
    CacheCleanupThread.join();
    ThumbnailGenerationThread.join();

    // Make sure all resources that use imagemagick are closed //
    // Clear cache //
    ImageCache.clear();
    LoadQueue.Clear();
    ThumbQueue.Clear();
}
// ------------------------------------ //
std::shared_ptr<LoadedImage> CacheManager::LoadFullImage(const std::string& file)
{
    std::lock_guard<std::mutex> lock(ImageCacheLock);

    auto cachedVersion = GetCachedImage(lock, file);

    if(cachedVersion)
        return cachedVersion;

    // Create new //
    auto created = std::make_shared<LoadedImage>(file);

    LOG_INFO("Opening full size image: " + file);

    // Add to cache //
    ImageCache.push_back(created);

    // Add it to load queue //
    {
        GUARD_LOCK_OTHER_NAME(LoadQueue, lock2);
        auto queuedTask = LoadQueue.Push(lock2, created);
        created->RegisterLoadTask(queuedTask);
    }

    LastCacheInsertTime = std::chrono::high_resolution_clock::now();

    // Notify loader thread //
    NotifyFullLoaderThread.notify_all();

    return created;
}

//! \brief Returns an image that will get the thumbnail for a file
//! \param hash Hash of the image file. Used to get the target thumbnail file name
std::shared_ptr<LoadedImage> CacheManager::LoadThumbImage(
    const std::string& file, const std::string& hash)
{
    LEVIATHAN_ASSERT(!hash.empty(), "LoadThumb called with empty hash");

    // Create new //
    auto created = std::make_shared<LoadedImage>(file);

    // Add it to load queue //
    {
        GUARD_LOCK_OTHER(ThumbQueue);
        auto queuedTask = ThumbQueue.Push(guard, std::make_tuple(created, hash));
        created->RegisterLoadTask(queuedTask);
    }

    // Notify loader thread //
    NotifyThumbnailGenerationThread.notify_all();

    return created;
}

std::shared_ptr<LoadedImage> CacheManager::CreateImageLoadFailure(
    const std::string& error) const
{
    const auto image = std::make_shared<LoadedImage>("ERROR");
    image->OnLoadFail(error);
    return image;
}
// ------------------------------------ //
std::shared_ptr<LoadedImage> CacheManager::GetCachedImage(
    const std::lock_guard<std::mutex>& lock, const std::string& file)
{
    for(const auto& cachedImage : ImageCache) {

        if(cachedImage->PathMatches(file))
            return cachedImage;
    }

    return nullptr;
}
// ------------------------------------ //
void CacheManager::NotifyMovedFile(const std::string& oldfile, const std::string& newfile)
{

    std::lock_guard<std::mutex> lock(ImageCacheLock);

    for(const auto& cachedImage : ImageCache) {

        if(cachedImage->PathMatches(oldfile))
            cachedImage->OnMoved(newfile);
    }
}

// ------------------------------------ //
// Resource loading
Glib::RefPtr<Gdk::Pixbuf> CacheManager::GetFolderIcon()
{
    std::lock_guard<std::mutex> lock(ResourceLoadMutex);

    if(FolderIcon)
        return FolderIcon;

    // Load it //
    FolderIcon = Gdk::Pixbuf::create_from_resource(
        "/com/boostslair/dualviewpp/resources/icons/file-folder.png");

    LEVIATHAN_ASSERT(FolderIcon, "Failed to load resource: FolderIcon");
    return FolderIcon;
}

Glib::RefPtr<Gdk::Pixbuf> CacheManager::GetCollectionIcon()
{
    std::lock_guard<std::mutex> lock(ResourceLoadMutex);

    if(CollectionIcon)
        return CollectionIcon;

    // Load it //
    CollectionIcon = Gdk::Pixbuf::create_from_resource(
        "/com/boostslair/dualviewpp/resources/icons/folders.png");

    LEVIATHAN_ASSERT(CollectionIcon, "Failed to load resource: CollectionIcon");
    return CollectionIcon;
}

std::shared_ptr<LoadedImage> CacheManager::GetFolderAsImage()
{
    std::unique_lock<std::mutex> lock(ResourceLoadMutex);

    if(FolderIconAsImage)
        return FolderIconAsImage;

    // Load it //
    FolderIconAsImage = std::make_shared<LoadedImage>(
        "resource:///com/boostslair/dualviewpp/resources/icons/file-folder.png");

    // Bit of a mess, but this is only done once
    lock.unlock();
    auto icon = GetFolderIcon();
    lock.lock();

    FolderIconAsImage->LoadFromGtkImage(icon);
    return FolderIconAsImage;
}

// ------------------------------------ //
void CacheManager::QuitProcessingThreads()
{
    Quitting = true;
}

// ------------------------------------ //
// Threads

void CacheManager::_RunFullSizeLoaderThread()
{
    GUARD_LOCK_OTHER(LoadQueue);

    while(!Quitting) {
        // Wait for more work //
        if(LoadQueue.Empty(guard))
            NotifyFullLoaderThread.wait(guard);

        // Process whole queue //
        while(auto current = LoadQueue.Pop(guard)) {

            // Unlock while loading the image file
            guard.unlock();

            current->Task->DoLoad();
            current->OnDone();

            guard.lock();
        }
    }
}

void CacheManager::_RunCacheCleanupThread()
{
    std::unique_lock<std::mutex> lock(CacheCleanupMutex);
    LastCacheInsertTime = std::chrono::high_resolution_clock::now();

    while(!Quitting) {

        NotifyCacheCleanup.wait_for(lock, std::chrono::seconds(10));

        {
            std::unique_lock<std::mutex> lock(ImageCacheLock);
            const auto time = std::chrono::high_resolution_clock::now();

            auto previous = LastCacheInsertTime.load(std::memory_order_acquire);
            const auto unloadAnywayTime =
                std::chrono::seconds(DUALVIEW_SETTINGS_UNLOAD_ANYWAY);
            bool useUnloadAnyway = time - previous > unloadAnywayTime;

            if(useUnloadAnyway) {
                LastCacheInsertTime.compare_exchange_weak(
                    previous, time, std::memory_order_acquire, std::memory_order_release);
            }

            for(auto iter = ImageCache.begin(); iter != ImageCache.end();) {

                const auto age = (time - (*iter)->GetLastUsed());

                // Expire //
                if((*iter).use_count() == 1 &&
                    age > std::chrono::milliseconds(DUALVIEW_SETTINGS_UNLOAD_TIME_MS)) {
                    // Unload it //
                    iter = ImageCache.erase(iter);
                } else if(useUnloadAnyway && age > unloadAnywayTime) {
                    iter = ImageCache.erase(iter);
                    // Only one per cycle
                    useUnloadAnyway = false;
                } else {

                    ++iter;
                }
            }

            // Force unload if too many
            int unloadsRemaining = 40;

            while(ImageCache.size() > DUALVIEW_SETTINGS_MAX_CACHED_IMAGES &&
                  unloadsRemaining > 0) {
                // TODO: not the optimal algorithm but oh well, this is run in the background
                auto oldest = ImageCache.begin();
                auto oldestAge = time - (*oldest)->GetLastUsed();

                for(auto iter = ImageCache.begin() + 1; iter != ImageCache.end(); ++iter) {
                    const auto age = (time - (*iter)->GetLastUsed());

                    if(age > oldestAge) {
                        oldest = iter;
                        oldestAge = age;
                    }
                }

                ImageCache.erase(oldest);
                --unloadsRemaining;
            }

            if(SHOW_IMAGE_CACHE_SIZE) {
                Logger::Get()->Info(
                    "Current image cache size is: " + std::to_string(ImageCache.size()));
            }
        }
    }
}

void CacheManager::_RunThumbnailGenerationThread()
{
    GUARD_LOCK_OTHER(ThumbQueue);

    while(!Quitting) {

        // Wait for more work //
        if(ThumbQueue.Empty(guard))
            NotifyThumbnailGenerationThread.wait(guard);

        // Process whole queue //
        while(auto current = ThumbQueue.Pop(guard)) {
            // Unlock while loading the image file
            guard.unlock();

            _LoadThumbnail(*std::get<0>(current->Task), std::get<1>(current->Task));
            current->OnDone();

            guard.lock();
        }
    }
}
// ------------------------------------ //
void CacheManager::_LoadThumbnail(LoadedImage& thumb, const std::string& hash) const
{
    // Get the thumbnail folder //
    const auto extension = boost::filesystem::extension(thumb.FromPath);

    if(extension.empty()) {

        LOG_WARNING(
            "Creating thumbnail for image with empty extension, full path: " + thumb.FromPath);

        // TODO: make a database upgrade guaranteeing all images have
        // an extensions and then remove this

        // Failed //
        // thumb.OnLoadFail("Filename has no extension"); return;
    }

    const auto target = boost::filesystem::path(DualView::Get().GetThumbnailFolder()) /
                        boost::filesystem::path(hash + extension);

    // Use already created thumbnail if one exists //
    if(boost::filesystem::exists(target)) {

        // Load the existing thumbnail //
        thumb.DoLoad(target.string());

        if(!thumb.IsValid()) {

            LOG_WARNING("Deleting invalid thumbnail: " + std::string(target.c_str()));
            boost::filesystem::remove(target);

            // TODO: avoid infinite recursion
            _LoadThumbnail(thumb, hash);
        }

        return;
    }

    // Load the full file //
    std::shared_ptr<std::vector<Magick::Image>> FullImage;

    try {

        LoadedImage::LoadImage(thumb.GetPath(), FullImage);

        if(!FullImage || FullImage->size() < 1)
            throw std::runtime_error("FullImage is null or empty");

    } catch(const std::exception& e) {

        const auto error =
            "Failed to open full image for thumbnail generation: " + std::string(e.what());
        LOG_ERROR(error + ", file: " + thumb.GetPath());
        thumb.OnLoadFail(error);
        return;
    }

    // Dispose of the actual image and store the thumbnail in memory //

    // Single frame image
    if(FullImage->size() < 2) {
        FullImage->at(0).resize(CreateResizeSizeForImage(FullImage->at(0), 128, 0));

        thumb.OnLoadSuccess(FullImage);
    } else {
        // This will remove the optimization and change the image to how it looks at that point
        // during the animation.
        // More info here: http://www.imagemagick.org/Usage/anim_basics/#coalesce

        // The images are already coalesced here //

        // TODO: maybe if the animation delay is over MAXIMUM_ALLOWED_ANIMATION_FRAME_DURATION
        // still remove frames?
        // TODO: always reduce the preview to be less than X number of frames? Or a fraction of
        // the original?
        if(FullImage->at(0).animationDelay() * 0.01f <
                MAXIMUM_ALLOWED_ANIMATION_FRAME_DURATION &&
            FullImage->size() > 10) {
            // Resize and remove every other frame //
            bool remove = false;

            for(size_t i = 0; i < FullImage->size();) {
                if(remove) {
                    FullImage->erase(FullImage->begin() + i);

                } else {
                    // Increase delay and resize //
                    size_t extradelay = 0;

                    if(i + 1 < FullImage->size()) {
                        extradelay = FullImage->at(i + 1).animationDelay();
                    }

                    FullImage->at(i).animationDelay(
                        FullImage->at(i).animationDelay() + extradelay);

                    FullImage->at(i).resize(
                        CreateResizeSizeForImage(FullImage->at(i), 128, 0));

                    ++i;
                }

                remove = !remove;
            }

        } else {
            // Just resize
            for(auto& frame : *FullImage) {

                frame.resize(CreateResizeSizeForImage(frame, 128, 0));
            }
        }

        thumb.OnLoadSuccess(FullImage);
    }

    // Save it to a file //
    Magick::writeImages(FullImage->begin(), FullImage->end(), target.c_str());

    int64_t size;

    try {
        size = boost::filesystem::file_size(thumb.GetPath());
    } catch(const boost::filesystem::filesystem_error& e) {
        LOG_ERROR("Failed to get generated thumbnail size: " + std::string(e.what()));
        return;
    }

    size /= 1024;

    LOG_INFO("Generated thumbnail for: " + thumb.GetPath() + " size: " + std::to_string(size) +
             " KiB");
}
// ------------------------------------ //
std::string CacheManager::CreateResizeSizeForImage(
    const Magick::Image& image, int width, int height)
{
    if(width <= 0 && height <= 0)
        throw Leviathan::InvalidArgument("Both width and height are 0 or under");

    if(width <= 0)
        width = (int)((float)height * image.columns() / image.rows());

    if(height <= 0)
        height = (int)((float)width * image.rows() / image.columns());

    std::stringstream stream;
    stream << width << "x" << height;
    return stream.str();
}

bool CacheManager::GetImageSize(
    const std::string& image, int& width, int& height, std::string& extension)
{
    try {
        Magick::Image img(image);

        const auto fileExtension = boost::filesystem::extension(image);

        if(fileExtension.empty()) {

            extension = img.magick();
            LEVIATHAN_ASSERT(!extension.empty(), "extension and magick format is empty");
        }

        width = img.columns();
        height = img.rows();

        return true;

    } catch(const std::exception& e) {

        LOG_ERROR("CacheManager: GetImageSize: exception: " + std::string(e.what()));
        return false;
    }
}

bool CacheManager::CheckIsBytesAnImage(const std::string& imagedata)
{
    try {
        Magick::Blob data(imagedata.c_str(), imagedata.size());

        Magick::Image img(data);

        const auto extension = img.magick();

        if(extension.empty())
            return false;

        return true;

    } catch(const std::exception& e) {

        LOG_WARNING("CacheManager: CheckIsBytesAnImage: failed with exception: " +
                    std::string(e.what()));
        return false;
    }
}

std::string CacheManager::GetFinalImagePath(const std::string& path)
{
    if(path.empty())
        return path;

    // substr is used to cut the beginning. -1 after sizeof because of nullterminator
    // Public collection
    if(Leviathan::StringOperations::StringStartsWith<std::string>(path, ":?ocl/")) {

        return (boost::filesystem::path(DualView::Get().GetSettings().GetPublicCollection()) /
                path.substr(sizeof(":?ocl/") - 1))
            .string();
    }

    if(Leviathan::StringOperations::StringStartsWith<std::string>(
           path, "./public_collection/")) {
        return (boost::filesystem::path(DualView::Get().GetSettings().GetPublicCollection()) /
                path.substr(sizeof("./public_collection/") - 1))
            .string();
    }


    // Private collection
    if(Leviathan::StringOperations::StringStartsWith<std::string>(path, ":?scl/")) {

        return (boost::filesystem::path(DualView::Get().GetSettings().GetPrivateCollection()) /
                path.substr(sizeof(":?scl/") - 1))
            .string();
    }

    if(Leviathan::StringOperations::StringStartsWith<std::string>(
           path, "./private_collection/")) {
        return (boost::filesystem::path(DualView::Get().GetSettings().GetPrivateCollection()) /
                path.substr(sizeof("./private_collection/") - 1))
            .string();
    }

    // Path is not in the database, don't touch it //
    return path;
}

std::string CacheManager::GetDatabaseImagePath(const std::string& path)
{
    const auto& settings = DualView::Get().GetSettings();

    if(Leviathan::StringOperations::StringStartsWith(path, settings.GetPrivateCollection())) {

        return ":?scl/" + path.substr(settings.GetPrivateCollection().size());
    }

    if(Leviathan::StringOperations::StringStartsWith(path, settings.GetPublicCollection())) {

        return ":?ocl/" + path.substr(settings.GetPublicCollection().size());
    }

    // That's an error //
    return "ERROR_DATABASIFYING:" + path;
}

// ------------------------------------ //
// LoadedImage
LoadedImage::LoadedImage(const std::string& path) : FromPath(path) {}

LoadedImage::~LoadedImage()
{
    MagickImage.reset();
}

void LoadedImage::UnloadImage()
{
    Status = IMAGE_LOAD_STATUS::Error;
    FromPath = "Forced unload";
    MagickImage.reset();
}
// ------------------------------------ //
void LoadedImage::LoadImage(
    const std::string& file, std::shared_ptr<std::vector<Magick::Image>>& image)
{
    if(!boost::filesystem::exists(file))
        throw Leviathan::InvalidArgument("File doesn't exist");

    auto createdImage = std::make_shared<std::vector<Magick::Image>>();

    if(!createdImage)
        throw Leviathan::InvalidArgument("Allocating image vector failed");

    // Load image //
    try {

        readImages(createdImage.get(), file);

    } catch(const Magick::Error& e) {

        // Loading failed //
        throw Leviathan::InvalidArgument(
            "Loaded image is invalid/unsupported: " + std::string(e.what()));
    }

    if(createdImage->empty())
        throw Leviathan::InvalidArgument("Loaded image is empty");

    // Coalesce animated images //
    if(createdImage->size() > 1) {

        image = std::make_shared<std::vector<Magick::Image>>();
        coalesceImages(image.get(), createdImage->begin(), createdImage->end());

        if(image->empty())
            throw Leviathan::InvalidArgument("Coalesced image is empty");

    } else {

        image = createdImage;
    }
}

void LoadedImage::DoLoad()
{
    try {

        LoadImage(FromPath, MagickImage);

        LEVIATHAN_ASSERT(MagickImage, "MagickImage is null after LoadImage, "
                                      "expected an exception");

        Status = IMAGE_LOAD_STATUS::Loaded;
        LoadTask.reset();

    } catch(const std::exception& e) {

        LOG_WARNING("LoadedImage: failed to load from path: " + FromPath);

        FromPath = "Error Loading: " + std::string(e.what());
        Status = IMAGE_LOAD_STATUS::Error;

        LOG_ERROR("Image failed to open from: " + FromPath + " error: " + e.what());
    }
}

void LoadedImage::DoLoad(const std::string& thumbfile)
{
    try {

        LoadImage(thumbfile, MagickImage);

        LEVIATHAN_ASSERT(MagickImage, "MagickImage is null after LoadImage, "
                                      "expected an exception");

        Status = IMAGE_LOAD_STATUS::Loaded;
        LoadTask.reset();

    } catch(const std::exception& e) {

        LOG_WARNING("LoadedImage: failed to load thumbnail from: " + thumbfile);

        FromPath = "Error Loading: " + std::string(e.what());
        Status = IMAGE_LOAD_STATUS::Error;
    }
}

void LoadedImage::OnLoadFail(const std::string& error)
{
    FromPath = error;
    Status = IMAGE_LOAD_STATUS::Error;
    LoadTask.reset();
}

void LoadedImage::OnLoadSuccess(std::shared_ptr<std::vector<Magick::Image>> image)
{
    LEVIATHAN_ASSERT(
        Status != IMAGE_LOAD_STATUS::Error, "OnLoadSuccess called on an errored image");

    LEVIATHAN_ASSERT(image, "OnLoadSuccess called with invalid image");

    MagickImage = image;
    Status = IMAGE_LOAD_STATUS::Loaded;
    LoadTask.reset();
}
// ------------------------------------ //
size_t LoadedImage::GetWidth() const
{
    if(!IsImageObjectLoaded())
        throw Leviathan::InvalidState("MagickImage not loaded");

    return MagickImage->front().columns();
}

size_t LoadedImage::GetHeight() const
{
    if(!IsImageObjectLoaded())
        throw Leviathan::InvalidState("MagickImage not loaded");

    return MagickImage->front().rows();
}

size_t LoadedImage::GetFrameCount() const
{
    if(!IsImageObjectLoaded())
        throw Leviathan::InvalidState("MagickImage not loaded");

    return MagickImage->size();
}

std::chrono::duration<float> LoadedImage::GetAnimationTime(size_t page) const
{
    if(!IsImageObjectLoaded())
        throw Leviathan::InvalidState("MagickImage not loaded");

    if(page >= MagickImage->size())
        throw Leviathan::InvalidArgument("page is outside valid range");

    auto delay = 0.01f * MagickImage->at(page).animationDelay();

    if(delay < MINIMUM_VALID_ANIMATION_FRAME_DURATION ||
        delay > MAXIMUM_ALLOWED_ANIMATION_FRAME_DURATION) {
        delay = DEFAULT_GIF_FRAME_DURATION;
    }

    return std::chrono::duration<float>(delay);
}
// ------------------------------------ //
Glib::RefPtr<Gdk::Pixbuf> LoadedImage::CreateGtkImage(size_t page /*= 0*/) const
{
    if(!IsImageObjectLoaded())
        throw Leviathan::InvalidState("MagickImage not loaded");

    if(page >= MagickImage->size())
        throw Leviathan::InvalidArgument("page is outside valid range");

    Magick::Image& image = MagickImage->at(page);

    const bool hasAlpha = false;
    const auto bitsPerSample = 8;
    const int width = image.columns();
    const int height = image.rows();

    const size_t stride = (bitsPerSample / 8) * 3 * width;

    // Create buffer //
    auto pixbuf = Gdk::Pixbuf::create(
        Gdk::Colorspace::COLORSPACE_RGB, hasAlpha, bitsPerSample, width, height);

    if(!pixbuf)
        throw Leviathan::Exception("Failed to create pixbuf");

    LEVIATHAN_ASSERT(pixbuf->get_width() == width, "Pixbuf wrong width created");
    LEVIATHAN_ASSERT(pixbuf->get_height() == height, "Pixbuf wrong height created");

    // It looks like Gtk might round up sizes for some reason
    LEVIATHAN_ASSERT(static_cast<size_t>(pixbuf->get_rowstride()) >= stride,
        "Gtk stride is unexpected, " + std::to_string(pixbuf->get_rowstride()) +
            " != " + std::to_string(stride));

    LEVIATHAN_ASSERT(pixbuf->get_byte_length() >= stride * height,
        "Magick and Gtk have different image sizes: " +
            std::to_string(pixbuf->get_byte_length()) +
            " != " + std::to_string(stride * height));

    // Copy data //
    unsigned char* destination = pixbuf->get_pixels();

    for(int y = 0; y < height; ++y) {
        image.write(0, y, width, 1, "RGB", Magick::CharPixel, destination);

        // destination += stride;
        destination += pixbuf->get_rowstride();
    }

    return pixbuf;
}

void LoadedImage::LoadFromGtkImage(Glib::RefPtr<Gdk::Pixbuf> image)
{
    LEVIATHAN_ASSERT(image->get_colorspace() == Gdk::COLORSPACE_RGB,
        "pixbuf format is different from expected");

    LEVIATHAN_ASSERT(
        image->get_bits_per_sample() == 8, "pixbuf has unexpected bits per sample: " +
                                               std::to_string(image->get_bits_per_sample()));

    Magick::Image newImage(image->get_width(), image->get_height(),
        image->get_has_alpha() ? "RGBA" : "RGB", Magick::CharPixel, image->get_pixels());

    MagickImage =
        std::make_shared<std::vector<Magick::Image>>(std::vector<Magick::Image>{newImage});

    Status = IMAGE_LOAD_STATUS::Loaded;
}
// ------------------------------------ //
void LoadedImage::BumpLoadPriority()
{
    if(IsLoaded() || !LoadTask)
        return;

    LoadTask->Bump();
}

void LoadedImage::RegisterLoadTask(std::shared_ptr<BaseTaskItem> loadTask)
{
    LoadTask = loadTask;
}
