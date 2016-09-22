// ------------------------------------ //
#include "CacheManager.h"

#include "Common.h"
#include <Magick++.h>
#include <gtkmm.h>

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

            
            
            lock.lock();
        }
        
        // Wait for more work //
        NotifyThumbnailGenerationThread.wait(lock);
    }
}




// ------------------------------------ //
// LoadedImage
LoadedImage::LoadedImage(const std::string &path){

}

LoadedImage::~LoadedImage(){

    MagickImage.reset();
}

void LoadedImage::UnloadImage(){

    Status = IMAGE_LOAD_STATUS::Error;
    FromPath = "Forced unload";
    MagickImage.reset();
}

