// ------------------------------------ //
#include "CacheManager.h"

#include "Common.h"
#include <Magick++.h>
#include <gtkmm.h>

#include "Exceptions.h"

#include <boost/filesystem.hpp>

using namespace DV;
// ------------------------------------ //
CacheManager::CacheManager(){

    Magick::InitializeMagick(Glib::get_current_dir().c_str());
    
    FullLoaderThread = std::thread(std::bind<void>(
            &CacheManager::_RunFullSizeLoaderThread, this));
    
    CacheCleanupThread = std::thread(std::bind<void>(
            &CacheManager::_RunCacheCleanupThread, this));

    ThumbnailGenerationThread = std::thread(std::bind<void>(
            &CacheManager::_RunThumbnailGenerationThread, this));
}

CacheManager::~CacheManager(){

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
    LoadQueue.clear();
    ThumbQueue.clear();
}
// ------------------------------------ //
std::shared_ptr<LoadedImage> CacheManager::LoadFullImage(const std::string &file){

    std::lock_guard<std::mutex> lock(ImageCacheLock);

    auto cachedVersion = GetCachedImage(lock, file);

    if(cachedVersion)
        return cachedVersion;

    // Create new //
    auto created = std::make_shared<LoadedImage>(file);

    // Add to cache //
    ImageCache.push_back(created);

    // Add it to load queue //
    {
        std::lock_guard<std::mutex> lock2(LoadQueueMutex);
        LoadQueue.push_back(created);
    }

    // Notify loader thread //
    NotifyFullLoaderThread.notify_all();

    return created;
}

//! \brief Returns an image that will get the thumbnail for a file
//! \param hash Hash of the image file. Used to get the target thumbnail file name
std::shared_ptr<LoadedImage> CacheManager::LoadThumbImage(const std::string &file,
    const std::string &hash)
{
    // Create new //
    auto created = std::make_shared<LoadedImage>(file);

    // Add to cache //
    ImageCache.push_back(created);

    // Add it to load queue //
    {
        std::lock_guard<std::mutex> lock(ThumbQueueMutex);
        ThumbQueue.push_back(created);
    }

    // Notify loader thread //
    NotifyThumbnailGenerationThread.notify_all();

    return created;
}

// ------------------------------------ //
std::shared_ptr<LoadedImage> CacheManager::GetCachedImage(
    const std::lock_guard<std::mutex> &lock, const std::string &file)
{
    for(const auto& cachedImage : ImageCache){

        if(cachedImage->PathMatches(file))
            return cachedImage;
    }

    return nullptr;
}
// ------------------------------------ //
void CacheManager::QuitProcessingThreads(){

    Quitting = true;
}

// ------------------------------------ //
// Threads

void CacheManager::_RunFullSizeLoaderThread(){

    std::unique_lock<std::mutex> lock(LoadQueueMutex);

    while(!Quitting){

        // Process whole queue //
        while(!LoadQueue.empty()){

            auto current = LoadQueue.front();
            LoadQueue.pop_front();

            // Unlock while loading the image file
            lock.unlock();

            current->DoLoad();
            
            lock.lock();
        }
        
        // Wait for more work //
        NotifyFullLoaderThread.wait(lock);
    }
}

void CacheManager::_RunCacheCleanupThread(){

    std::unique_lock<std::mutex> lock(CacheCleanupMutex);
    
    while(!Quitting){

        {
            std::unique_lock<std::mutex> lock(ImageCacheLock);
            const auto time = std::chrono::high_resolution_clock::now();

            for(auto iter = ImageCache.begin(); iter != ImageCache.end(); ){

                // Expire //
                if((*iter).use_count() == 1 &&
                    (time - (*iter)->GetLastUsed()) >
                    std::chrono::milliseconds(DUALVIEW_SETTINGS_UNLOAD_TIME_MS))
                {
                    // Unload it //
                    iter = ImageCache.erase(iter);
                    
                } else {

                    ++iter;
                }
            }
        }

        NotifyCacheCleanup.wait_for(lock, std::chrono::seconds(10));
    }
}

void CacheManager::_RunThumbnailGenerationThread(){

    std::unique_lock<std::mutex> lock(ThumbQueueMutex);
    
    while(!Quitting){

        // Process whole queue //
        while(!ThumbQueue.empty()){

            auto current = ThumbQueue.front();
            ThumbQueue.pop_front();

            // Unlock while loading the image file
            lock.unlock();

            LOG_INFO("DEBUG_BREAK thumbnail");
            DEBUG_BREAK;
            
            lock.lock();
        }
        
        // Wait for more work //
        NotifyThumbnailGenerationThread.wait(lock);
    }
}




// ------------------------------------ //
// LoadedImage
LoadedImage::LoadedImage(const std::string &path) : FromPath(path){

}

LoadedImage::~LoadedImage(){

    MagickImage.reset();
}

void LoadedImage::UnloadImage(){

    Status = IMAGE_LOAD_STATUS::Error;
    FromPath = "Forced unload";
    MagickImage.reset();
}
// ------------------------------------ //
void LoadedImage::LoadImage(const std::string &file,
    std::shared_ptr<std::vector<Magick::Image>> &image)
{
    if(!boost::filesystem::exists(file))
        throw Leviathan::InvalidArgument("File doesn't exist");
    
    auto createdImage = std::make_shared<std::vector<Magick::Image>>();

    if(!createdImage)
        throw Leviathan::InvalidArgument("Allocating image vector failed");

    // Load image //
    readImages(createdImage.get(), file);

    if(createdImage->empty())
        throw Leviathan::InvalidArgument("Loaded image is empty");

    // Coalesce animated images //
    if(createdImage->size() > 1){

        image = std::make_shared<std::vector<Magick::Image>>();
        coalesceImages(image.get(), createdImage->begin(), createdImage->end());

        if(image->empty())
            throw Leviathan::InvalidArgument("Coalesced image is empty");
        
    } else {

        image = createdImage;
    }
}

void LoadedImage::DoLoad(){

    try{

        LoadImage(FromPath, MagickImage);

        LEVIATHAN_ASSERT(MagickImage, "MagickImage is null after LoadImage, "
            "expected an exception");

        Status = IMAGE_LOAD_STATUS::Loaded;

    } catch(const std::exception &e){

        LOG_WARNING("LoadedImage: failed to load from path: " + FromPath);

        FromPath = "Error Loading: " + std::string(e.what());
        Status = IMAGE_LOAD_STATUS::Error;
    }
}
// ------------------------------------ //
size_t LoadedImage::GetWidth() const{
    
    if(!IsImageObjectLoaded())
        throw Leviathan::InvalidState("MagickImage not loaded");
    
    return MagickImage->front().columns();
}

size_t LoadedImage::GetHeight() const{
    
    if(!IsImageObjectLoaded())
        throw Leviathan::InvalidState("MagickImage not loaded");
    
    return MagickImage->front().rows();
}

size_t LoadedImage::GetFrameCount() const{

    if(!IsImageObjectLoaded())
        throw Leviathan::InvalidState("MagickImage not loaded");

    return MagickImage->size();
}

std::chrono::duration<float> LoadedImage::GetAnimationTime(size_t page) const{
    
    if(!IsImageObjectLoaded())
        throw Leviathan::InvalidState("MagickImage not loaded");

    if(page >= MagickImage->size())
        throw Leviathan::InvalidArgument("page is outside valid range");

    return std::chrono::duration<float>(0.01f * MagickImage->at(page).animationDelay());
}
// ------------------------------------ //
Glib::RefPtr<Gdk::Pixbuf> LoadedImage::CreateGtkImage(size_t page /*= 0*/) const{

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
    auto pixbuf = Gdk::Pixbuf::create(Gdk::Colorspace::COLORSPACE_RGB,
        hasAlpha, bitsPerSample, width, height);

    if(!pixbuf)
        throw Leviathan::Exception("Failed to create pixbuf");

    LEVIATHAN_ASSERT(pixbuf->get_width() == width, "Pixbuf wrong width created");
    LEVIATHAN_ASSERT(pixbuf->get_height() == height, "Pixbuf wrong height created");

    // It looks like Gtk might round up sizes for some reason
    LEVIATHAN_ASSERT(static_cast<size_t>(pixbuf->get_rowstride())
        >= stride, "Gtk stride is unexpected, " + std::to_string(pixbuf->get_rowstride()) +
        " != " + std::to_string(stride));

    LEVIATHAN_ASSERT(pixbuf->get_byte_length() >= stride * height,
        "Magick and Gtk have different image sizes: " +
        std::to_string(pixbuf->get_byte_length()) + " != " + std::to_string(stride * height));

    // Copy data //
    unsigned char* destination = pixbuf->get_pixels();
    
    for(int y = 0; y < height; ++y)
    {
        image.write(0, y, width, 1, "RGB", Magick::CharPixel, destination);

        //destination += stride;
        destination += pixbuf->get_rowstride();
    }

    return pixbuf;
}

