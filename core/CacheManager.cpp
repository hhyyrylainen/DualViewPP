// ------------------------------------ //
#include "CacheManager.h"

#include "Common.h"
#include <Magick++.h>
#include <gtkmm.h>

#include "Exceptions.h"

#include "DualView.h"
#include "Settings.h"

#include "leviathan/Common/StringOperations.h"

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

    LOG_INFO("Opening full size image: " + file);

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
    LEVIATHAN_ASSERT(!hash.empty(), "LoadThumb called with empty hash");
    
    // Create new //
    auto created = std::make_shared<LoadedImage>(file);

    // Add it to load queue //
    {
        std::lock_guard<std::mutex> lock(ThumbQueueMutex);
        ThumbQueue.push_back(std::make_tuple(created, hash));
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
void CacheManager::NotifyMovedFile(const std::string &oldfile, const std::string &newfile){

    std::lock_guard<std::mutex> lock(ImageCacheLock);

    for(const auto& cachedImage : ImageCache){

        if(cachedImage->PathMatches(oldfile))
            cachedImage->OnMoved(newfile);
    }
}

// ------------------------------------ //
// Resource loading
Glib::RefPtr<Gdk::Pixbuf> CacheManager::GetFolderIcon(){

    std::lock_guard<std::mutex> lock(ResourceLoadMutex);

    if(FolderIcon)
        return FolderIcon;

    // Load it //
    FolderIcon = Gdk::Pixbuf::create_from_file(
        DualView::Get().GetSettings().GetPathToFolderIcon());

    LEVIATHAN_ASSERT(FolderIcon, "Failed to load resource: FolderIcon");
    return FolderIcon;
}

Glib::RefPtr<Gdk::Pixbuf> CacheManager::GetCollectionIcon(){

    std::lock_guard<std::mutex> lock(ResourceLoadMutex);

    if(CollectionIcon)
        return CollectionIcon;

    // Load it //
    CollectionIcon = Gdk::Pixbuf::create_from_file(
        DualView::Get().GetSettings().GetPathToCollectionIcon());

    LEVIATHAN_ASSERT(CollectionIcon, "Failed to load resource: CollectionIcon");
    return CollectionIcon;
}

std::shared_ptr<LoadedImage> CacheManager::GetFolderAsImage(){

    std::lock_guard<std::mutex> lock(ResourceLoadMutex);

    if(FolderIconAsImage)
        return FolderIconAsImage;

    // Load it //
    FolderIconAsImage = std::make_shared<LoadedImage>(
        DualView::Get().GetSettings().GetPathToFolderIcon());
    
    {
        std::lock_guard<std::mutex> lock2(LoadQueueMutex);
        LoadQueue.push_back(FolderIconAsImage);
    }

    // Notify loader thread //
    NotifyFullLoaderThread.notify_all();
    
    return FolderIconAsImage;
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

        // Wait for more work //
        if(LoadQueue.empty())
            NotifyFullLoaderThread.wait(lock);

        // Process whole queue //
        while(!LoadQueue.empty()){

            auto current = LoadQueue.front();
            LoadQueue.pop_front();

            // Unlock while loading the image file
            lock.unlock();

            current->DoLoad();
            
            lock.lock();
        }
    }
}

void CacheManager::_RunCacheCleanupThread(){

    std::unique_lock<std::mutex> lock(CacheCleanupMutex);
    
    while(!Quitting){

        NotifyCacheCleanup.wait_for(lock, std::chrono::seconds(10));
        
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
    }
}

void CacheManager::_RunThumbnailGenerationThread(){

    std::unique_lock<std::mutex> lock(ThumbQueueMutex);
    
    while(!Quitting){

        // Wait for more work //
        if(ThumbQueue.empty())
            NotifyThumbnailGenerationThread.wait(lock);

        // Process whole queue //
        while(!ThumbQueue.empty()){

            auto current = ThumbQueue.front();
            ThumbQueue.pop_front();

            // Unlock while loading the image file
            lock.unlock();

            _LoadThumbnail(*std::get<0>(current), std::get<1>(current));
            
            lock.lock();
        }
    }
}
// ------------------------------------ //
void CacheManager::_LoadThumbnail(LoadedImage &thumb, const std::string &hash) const{

    // Get the thumbnail folder //
    const auto extension = boost::filesystem::extension(thumb.FromPath);

    if(extension.empty()){

        LOG_WARNING("Creating thumbnail for image with empty extension");

        // TODO: make a database upgrade guaranteeing all images have
        // an extensions and then remove this

        // Failed //
        // thumb.OnLoadFail("Filename has no extension"); return;
    }
    
    const auto target = boost::filesystem::path(DualView::Get().GetThumbnailFolder()) /
        boost::filesystem::path(hash + extension);

    // Use already created thumbnail if one exists //
    if(boost::filesystem::exists(target)){

        // Load the existing thumbnail //
        thumb.DoLoad(target.c_str());

        if(!thumb.IsValid()){

            LOG_WARNING("Deleting invalid thumbnail: " + std::string(target.c_str()));
            boost::filesystem::remove(target);
            _LoadThumbnail(thumb, hash);
        }
        
        return;
    }

    // Load the full file //
    std::shared_ptr<std::vector<Magick::Image>> FullImage;

    try{

        LoadedImage::LoadImage(thumb.GetPath(), FullImage);

        if(!FullImage || FullImage->size() < 1)
            throw std::runtime_error("FullImage is null or empty");
        
    } catch(const std::exception &e){

        const auto error = "Failed to open full image for thumbnail generation: " +
            std::string(e.what());
        LOG_ERROR(error + ", file: " + thumb.GetPath());
        thumb.OnLoadFail(error);
        return;
    }

    // Dispose of the actual image and store the thumbnail in memory //

    // Single frame image
    if (FullImage->size() < 2)
    {
        FullImage->at(0).resize(CreateResizeSizeForImage(FullImage->at(0), 128, 0));

        thumb.OnLoadSuccess(FullImage);
    }
    else
    {
        // This will remove the optimization and change the image to how it looks at that point
        // during the animation.
        // More info here: http://www.imagemagick.org/Usage/anim_basics/#coalesce

        // The images are already coalesced here //
        
        if(FullImage->at(0).animationDelay() < 25 && FullImage->size() > 10)
        {
            // Resize and remove every other frame //
            bool remove = false;
                    
            for(size_t i = 0; i < FullImage->size(); )
            {
                if (remove)
                {
                    FullImage->erase(FullImage->begin() + i);

                } else
                {
                    // Increase delay and resize //
                    size_t extradelay = 0;

                    if (i + 1 < FullImage->size())
                    {
                        extradelay = FullImage->at(i + 1).animationDelay();
                    }

                    FullImage->at(i).animationDelay(FullImage->at(i).animationDelay() +
                                                   extradelay);
                    
                    FullImage->at(i).resize(
                        CreateResizeSizeForImage(FullImage->at(i), 128, 0));

                    ++i;
                }

                remove = !remove;
            }

        } else
        {
            // Just resize
            for(auto& frame : *FullImage){

                frame.resize(CreateResizeSizeForImage(frame, 128, 0));
            }
        }

        thumb.OnLoadSuccess(FullImage);
    }

    // Save it to a file //
    Magick::writeImages(FullImage->begin(), FullImage->end(), target.c_str());

    LOG_INFO("Generated thumbnail for: " + thumb.GetPath());
}
// ------------------------------------ //
std::string CacheManager::CreateResizeSizeForImage(const Magick::Image &image, int width,
    int height)
{
    if (width <= 0 && height <= 0)
        throw Leviathan::InvalidArgument("Both width and height are 0 or under");

    // TODO: verify that this width calculation is correct
    if (width <= 0)
        width = (int)((float)height * image.columns() / image.rows());

    if (height <= 0)
        height = (int)((float)width * image.rows() / image.columns());

    std::stringstream stream;
    stream << width << "x" << height;
    return stream.str();
}

bool CacheManager::GetImageSize(const std::string &image, int &width, int &height,
    std::string &extension)
{
    try{

        Magick::Image img(image);

        const auto fileExtension = boost::filesystem::extension(image);

        if(fileExtension.empty()){

            extension = img.magick();
            LEVIATHAN_ASSERT(!extension.empty(), "extension and magick format is empty");
        }

        width = img.columns();
        height = img.rows();
        
        return true;

    } catch(const std::exception &e){

        LOG_ERROR("CacheManager: GetImageSize: exception: " + std::string(e.what()));
        return false;
    }
}

std::string CacheManager::GetFinalImagePath(const std::string &path){

    if(path.empty())
        return path;

    // substr is used to cut the beginning. -1 after sizeof because of nullterminator
    // Public collection
    if(Leviathan::StringOperations::StringStartsWith<std::string>(path, ":?ocl/")){
        
        return (boost::filesystem::path(DualView::Get().GetSettings().GetPublicCollection()) /
            path.substr(sizeof(":?ocl/") - 1)).string();
    }

    if(Leviathan::StringOperations::StringStartsWith<std::string>(path,
            "./public_collection/"))
    {
        return (boost::filesystem::path(DualView::Get().GetSettings().GetPublicCollection()) /
            path.substr(sizeof("./public_collection/") - 1)).string();
    }


    // Private collection
    if(Leviathan::StringOperations::StringStartsWith<std::string>(path, ":?scl/")){
        
        return (boost::filesystem::path(DualView::Get().GetSettings().GetPrivateCollection()) /
            path.substr(sizeof(":?scl/") - 1)).string();
    }

    if(Leviathan::StringOperations::StringStartsWith<std::string>(path,
            "./private_collection/"))
    {
        return (boost::filesystem::path(DualView::Get().GetSettings().GetPrivateCollection()) /
            path.substr(sizeof("./private_collection/") - 1)).string();
    }
    
    // Path is not in the database, don't touch it //
    return path;
}

std::string CacheManager::GetDatabaseImagePath(const std::string &path){


    const auto& settings = DualView::Get().GetSettings();

    if(Leviathan::StringOperations::StringStartsWith(path, settings.GetPrivateCollection())){

        return ":?scl/" + path.substr(settings.GetPrivateCollection().size());
    }

    if(Leviathan::StringOperations::StringStartsWith(path, settings.GetPublicCollection())){

        return ":?ocl/" + path.substr(settings.GetPublicCollection().size());
    }
    
    // That's an error //
    return "ERROR_DATABASIFYING:" + path;
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
    try{
                
        readImages(createdImage.get(), file);
                
    } catch(const Magick::Error &e){

        // Loading failed //
        throw Leviathan::InvalidArgument("Loaded image is invalid/unsupported: " + 
            std::string(e.what()));
    }

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

        LOG_ERROR("Image failed to open from: " + FromPath + " error: " + e.what());
    }
}

void LoadedImage::DoLoad(const std::string &thumbfile){

    try{

        LoadImage(thumbfile, MagickImage);

        LEVIATHAN_ASSERT(MagickImage, "MagickImage is null after LoadImage, "
            "expected an exception");

        Status = IMAGE_LOAD_STATUS::Loaded;

    } catch(const std::exception &e){

        LOG_WARNING("LoadedImage: failed to load thumbnail from: " + thumbfile);

        FromPath = "Error Loading: " + std::string(e.what());
        Status = IMAGE_LOAD_STATUS::Error;
    }
}

void LoadedImage::OnLoadFail(const std::string &error){

    FromPath = error;
    Status = IMAGE_LOAD_STATUS::Error;
}

void LoadedImage::OnLoadSuccess(std::shared_ptr<std::vector<Magick::Image>> image){

    LEVIATHAN_ASSERT(Status != IMAGE_LOAD_STATUS::Error,
        "OnLoadSuccess called on an errored image");

    LEVIATHAN_ASSERT(image, "OnLoadSuccess called with invalid image");

    MagickImage = image;
    Status = IMAGE_LOAD_STATUS::Loaded;
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

