#pragma once

#include "TaskListWithPriority.h"

#include "Exceptions.h"

#include <gdkmm/pixbuf.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

namespace Magick {

class Image;
}

namespace DV {

constexpr auto SHOW_IMAGE_CACHE_SIZE = false;

class CacheManager;

//! \brief Holds an image that has been loaded into memory
class LoadedImage {
    friend CacheManager;

public:
    //! \brief Status of image
    enum class IMAGE_LOAD_STATUS {
        //! The object has just been created and is waiting
        Waiting,
        //! The image is loaded correctly
        Loaded,
        //! An error occured while loading
        Error,
        //! The image is running the load callback
        // Running_Callback
    };

public:
    ~LoadedImage();

    //! \brief Returns true if this image is no longer waiting
    inline bool IsLoaded() const
    {
        return Status != IMAGE_LOAD_STATUS::Waiting;
    }

    //! \brief Bumps the priority of loading this image, only effective if !IsLoaded
    void BumpLoadPriority();

    //! \brief Returns true if loading was successfull
    inline bool IsValid() const
    {
        return MagickImage && (Status == IMAGE_LOAD_STATUS::Loaded);
    }

    //! \brief Returns true if MagickImage is loaded
    inline bool IsImageObjectLoaded() const
    {
        return MagickImage.operator bool();
    }

    //! \brief Returns true if path matches the path that this image has loaded
    inline bool PathMatches(const std::string& path) const
    {
        return Status != IMAGE_LOAD_STATUS::Error && FromPath == path;
    }

    //! \brief Called when the image that is in this object is moved
    void OnMoved(const std::string& newfile)
    {
        // Don't erase error message
        if(Status == IMAGE_LOAD_STATUS::Error)
            return;

        FromPath = newfile;
    }

    //! \brief Resets the last use time
    inline void ResetActiveTime()
    {
        LastUsed = std::chrono::high_resolution_clock::now();
    }

    //! \brief Returns the time when ResetActiveTime was last called
    inline auto GetLastUsed() const
    {
        return LastUsed;
    }

    //! \brief Returns the path or empty if the path has been replaced by an error message
    inline std::string GetPath() const
    {
        if(Status == IMAGE_LOAD_STATUS::Error)
            return "error has occured";

        return FromPath;
    }

    //! \brief Returns the error message if IsValid is false
    std::string GetError() const
    {
        if(Status != IMAGE_LOAD_STATUS::Error)
            return "no error";

        return FromPath;
    }

    //! \brief Returns the width of the image
    //! \exception Leviathan::InvalidState if no image loaded
    size_t GetWidth() const;

    //! \brief Returns the width of the image
    //! \exception Leviathan::InvalidState if no image loaded
    size_t GetHeight() const;

    //! \brief Returns the number of frames in the image
    //! \exception Leviathan::InvalidState if no image loaded
    size_t GetFrameCount() const;


    //! \brief Returns the time current frame should be shown for
    std::chrono::duration<float> GetAnimationTime(size_t page) const;

    //! \brief Creates a gtk image for drawing
    //! \param page The page to get the image for. 0 for first image.
    //! GetFrameCount() - 1 for last image.
    //! \exception Leviathan::InvalidArgument if page is outside valid range
    //! \exception Leviathan::InvalidState if no image loaded
    //! \note It looks like Gtk rounds up sizes for some images,
    //! but they seem to work just fine
    //! \todo Alpha support
    Glib::RefPtr<Gdk::Pixbuf> CreateGtkImage(size_t page = 0) const;

    //! \brief Registers the task this waits on
    void RegisterLoadTask(std::shared_ptr<BaseTaskItem> loadTask);

    //! \brief Returns the internal magick image.
    //!
    //! This should only be used if this class doesn't provide some
    //! required Magick function
    std::shared_ptr<std::vector<Magick::Image>> GetMagickImage()
    {
        return MagickImage;
    }

    //! \brief Loads an image from file to the Magick++ object
    //! \exception Leviathan::InvalidArgument If the file couldn't be loaded
    static void LoadImage(
        const std::string& file, std::shared_ptr<std::vector<Magick::Image>>& image);

public:
    //! \brief Create new LoadedImage
    //! \protected
    //! \warning Don't call this from anywhere else than CacheManager
    explicit LoadedImage(const std::string& path);

protected:
    //! \brief Loads this from a Gtk image
    void LoadFromGtkImage(Glib::RefPtr<Gdk::Pixbuf> image);

    //! \brief Loads this image
    //!
    //! Called in a worker thread by CacheManager
    void DoLoad();

    //! \brief Loads a thumbnail into this object
    //!
    //! Called by the thumbnail thread
    void DoLoad(const std::string& thumbfile);

    //! \brief When an error occurs this is called by the loader thread
    void OnLoadFail(const std::string& error);

    //! \brief Called by CacheManager when this is properly loaded
    void OnLoadSuccess(std::shared_ptr<std::vector<Magick::Image>> image);

    //! \brief Forcefully unloads MagickImage.
    //!
    //! Sets error to "Force unloaded"
    void UnloadImage();

protected:
    //! Used to unload old images
    std::chrono::high_resolution_clock::time_point LastUsed =
        std::chrono::high_resolution_clock::now();

    IMAGE_LOAD_STATUS Status = IMAGE_LOAD_STATUS::Waiting;

    std::shared_ptr<BaseTaskItem> LoadTask;

    //! The path this was loaded from. Or the error message
    std::string FromPath;

    //! The magick image objects
    //! \todo Check if std::vector gives better performance
    std::shared_ptr<std::vector<Magick::Image>> MagickImage;
};

//! \brief Manages loading images
//! \note This class will perform ImageMagick initialization automatically
class CacheManager {
public:
    //! \brief Readies ImageMagick to be used by this instance
    CacheManager();

    //! \note It seems that there is no close method in Magick++ so that isn't called
    ~CacheManager();

    //! \brief Returns an image that will get a full image once loaded
    std::shared_ptr<LoadedImage> LoadFullImage(const std::string& file);

    //! \brief Returns an image that will get the thumbnail for a file
    //! \param hash Hash of the image file. Used to get the target thumbnail file name
    //! \todo Test if this should also be cached
    std::shared_ptr<LoadedImage> LoadThumbImage(
        const std::string& file, const std::string& hash);

    //! \brief Creates a LoadedImage that is in failed state
    //!
    //! Used if the actual file to load can't be detected and an error LoadedImage is needed
    std::shared_ptr<LoadedImage> CreateImageLoadFailure(const std::string& error) const;

    //! \brief Returns a full image from the cache
    inline std::shared_ptr<LoadedImage> GetCachedImage(const std::string& file)
    {
        std::lock_guard<std::mutex> lock(ImageCacheLock);
        return GetCachedImage(lock, file);
    }

    //! \brief Called when a file is moved, updates cache references to that file
    void NotifyMovedFile(const std::string& oldfile, const std::string& newfile);


    // Resource loading

    //! Icon for folders
    Glib::RefPtr<Gdk::Pixbuf> GetFolderIcon();

    Glib::RefPtr<Gdk::Pixbuf> GetCollectionIcon();

    //! Folder as a foreground image
    std::shared_ptr<LoadedImage> GetFolderAsImage();

    //! \brief Marks the processing threads as quitting
    void QuitProcessingThreads();

    //! \brief Calculates the parameter for magick image resize
    static std::string CreateResizeSizeForImage(
        const int currentWidth, const int currentHeight, int targetWidth, int targetHeight);

    //! \brief Helper variant that extracts image size from image
    static std::string CreateResizeSizeForImage(
        const Magick::Image& image, int targetWidth, int targetHeight);

    //! \brief Loads an image and looks up the size then unloads it
    //! \returns False if the image cannot be opened
    static bool GetImageSize(
        const std::string& image, int& width, int& height, std::string& extension);

    //! \brief Returns true if a byte string is an image
    //! \note Probably not the worlds fastest implementation as this opens the whole thing
    //! as a Magick::Image
    static bool CheckIsBytesAnImage(const std::string& imagedata);

    //! \brief Returns the actual path for loading an image. This takes the database directory
    //! in account
    static std::string GetFinalImagePath(const std::string& path);

    //! \brief Returns a database version for a path
    static std::string GetDatabaseImagePath(const std::string& path);

protected:
    std::shared_ptr<LoadedImage> GetCachedImage(
        const std::lock_guard<std::mutex>& lock, const std::string& file);

    void _RunFullSizeLoaderThread();

    void _RunCacheCleanupThread();

    void _RunThumbnailGenerationThread();

    //! \brief Loads a thumbnail
    //!
    //! The thumnail will be created if it doesn't exist already
    void _LoadThumbnail(LoadedImage& thumb, const std::string& hash) const;


protected:
    //! When set to true the loader threads will quit
    std::atomic<bool> Quitting = {false};

    //! Contains recently open images and maybe the next image if the user hasn't
    //! changed an image in a while. Will be periodically cleared by _RunCacheCleanupThread
    std::vector<std::shared_ptr<LoadedImage>> ImageCache;

    //! Lock when using ImageCache
    std::mutex ImageCacheLock;

    //! Time since something was added to image cache, used to clear out the cache when idle
    std::atomic<std::chrono::high_resolution_clock::time_point> LastCacheInsertTime;

    // FullLoader //
    //! Uses LoadQueueMutex for locking
    std::condition_variable NotifyFullLoaderThread;
    std::thread FullLoaderThread;

    TaskListWithPriority<std::shared_ptr<LoadedImage>> LoadQueue;

    // CacheCleanup //
    std::condition_variable NotifyCacheCleanup;
    std::thread CacheCleanupThread;
    std::mutex CacheCleanupMutex;

    // Thumbnail Generator //
    //! Uses ThumbQueueMutex for locking
    std::condition_variable NotifyThumbnailGenerationThread;
    std::thread ThumbnailGenerationThread;

    //! List of thumbnails that need to be loaded. The string in the tuple is the
    //! file hash
    TaskListWithPriority<std::tuple<std::shared_ptr<LoadedImage>, std::string>> ThumbQueue;

    // Resource managing //

    // For simplicity resources are loaded the first time they are requested

    //! Locked when getting resources to make sure there's no issues updating the pointers
    std::mutex ResourceLoadMutex;

    Glib::RefPtr<Gdk::Pixbuf> FolderIcon;
    Glib::RefPtr<Gdk::Pixbuf> CollectionIcon;


    //! The folder icon as an image
    std::shared_ptr<LoadedImage> FolderIconAsImage;
};


} // namespace DV
