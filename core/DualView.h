#pragma once
#include <gtkmm.h>

#include "windows/BaseWindow.h"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>


namespace Leviathan{

class Logger;
}

namespace DV{

class Image;
class TagCollection;
class Collection;

class PluginManager;
class CacheManager;
class CurlWrapper;
class Database;

class Settings;

class CollectionView;

//! \brief Main class that contains all the windows and systems
class DualView {
public:

    //! \brief Loads the GUI layout files and starts
    DualView(Glib::RefPtr<Gtk::Application> app);

    //! \brief Closes all resources and then attempts to close all open windows
    ~DualView();

    //! \brief Opens an image viewer for a file
    //! \returns True if opened. False if the file isn't supported
    bool OpenImageViewer(const std::string &file);

    //! \brief Registers a gtk window with the gtk instance
    void RegisterWindow(Gtk::Window &window);

    //! \brief Adds a closed message to the queue and invokes the main thread
    //! \param event The event that has a pointer to the closed window. This
    //! won't be dereferenced.
    //! \note This usually gets called twice when closing windows
    void WindowClosed(std::shared_ptr<WindowClosedEvent> event);

    //! \brief Add cmd to a queue to be ran on the main thread.
    //! \note Won't notify the main thread. So it must be notified
    //! through another function
    //! \todo Add that notify function
    void QueueCmd(std::function<void (DualView&)> cmd);

    //! \brief Adds an image to hash calculation queue
    //!
    //! After the image is added its hash will be calculated on a worker thread.
    //! If the image was already in the database it the passed in image will be made
    //! into a duplicate of the existing image
    void QueueImageHashCalculate(std::shared_ptr<Image> img);

    //! \brief Queues a function to be ran on the database thread
    void QueueDBThreadFunction(std::function<void()> func);

    //! \brief Returns a path to the collection root folder, where imported images are
    //! copied or moved to
    std::string GetPathToCollection(bool isprivate) const;

    //! \brief Makes a target file path shorter than DUALVIEW_MAX_ALLOWED_PATH and one
    //! that doesn't exist
    static std::string MakePathUniqueAndShort(const std::string &path);
    
    //! \brief Moves an image to the folder determined from the collection's name
    //! \return True if succeeded, false if it failed for some reason
    //! \param move If true the file will be moved. If false the file will be copied instead
    bool MoveFileToCollectionFolder(std::shared_ptr<Image> img,
        std::shared_ptr<Collection> collection, bool move);
    
    //
    // Database insert and modify functions
    //
    
    //! \brief Imports images to the database and adds them to the collection
    //! \param move If true the original file is deleted (only if  the file is not in the
    //! collection folder)
    bool AddToCollection(std::vector<std::shared_ptr<Image>> resources, bool move,
        std::string collectionname, const TagCollection &addcollectiontags,
        std::function<void(float)> progresscallback = nullptr);    

    //! \brief Retrieves a Collection from the database by name
    std::shared_ptr<Collection> GetOrCreateCollection(const std::string &name, bool isprivate);
    
    //
    // Database object retrieve functions
    //
    

    
    //! \brief Returns the thumbnail folder
    std::string GetThumbnailFolder() const;

    //! \brief Returns the CacheManager. use to load images
    //! \todo Assert if _CacheManager is null
    inline CacheManager& GetCacheManager() const{

        return *_CacheManager;
    }

    //! \brief Returns the Database. Query all data from here
    //! \todo Assert if _Database is null
    inline Database& GetDatabase() const{

        return *_Database;
    }

    //! \brief Returns settings
    //! \todo Assert if _Settings is null
    inline Settings& GetSettings() const{

        return *_Settings;
    }

    //! \brief Returns settings
    inline Leviathan::Logger* GetLogger() const{

        return _Logger.get();
    }
    
    //! \brief Returns true if called on the main thread
    //!
    //! Used to detect errors where functions are called on the wrong thread
    static bool IsOnMainThread();
    
    //! \brief Returns the global instance or asserts and quits the program
    static DualView& Get();

protected:

    //! \brief Constructor for test subclass to use
    DualView(bool tests);

    //! \brief Constructor for test subclass to use
    DualView(std::string tests);

    //! \brief Ran in the loader thread
    void _RunInitThread();

    //! \brief The Actual load function used by _RunInitThread
    //! \returns True if an error occured
    bool _DoInitThreadAction();

    //! \brief Called in the main thread once loading has completed
    //! \param succeeded True if there were no errors in the loading thread
    //! \todo Show load error to user
    void _OnLoadingFinished();

    //! \brief Called when messages are received to handle them
    void _HandleMessages();

    //! \brief Adds a new window to the open list
    //!
    //! This is needed to make sure that they aren't deallocated immediately
    void _AddOpenWindow(std::shared_ptr<BaseWindow> window);

    //! \brief Handles all queued commandline arguments
    void _ProcessCmdQueue();

    //! \brief Processes hash calculation queue
    //! \todo Make this return images that are duplicates of currently loaded images,
    //! that are loaded but aren't in the database
    void _RunHashCalculateThread();

    //! \brief Processes DatabaseFuncQueue
    void _RunDatabaseThread();
    
private:

    // Gtk callbacks
    void OpenImageFile_OnClick();

    void OpenCollection_OnClick();

    //! Once an instance is loaded init can start properly
    void _OnInstanceLoaded();

    //! \brief Extra instances will pass parameters here
    //! \returns The exit code for the extra instance
    int _HandleCmdLine(const Glib::RefPtr<Gio::ApplicationCommandLine> &command_line);


    int _OnPreParseCommandLine(const Glib::RefPtr<Glib::VariantDict> &options);

    //! \brief Receives a list of files to open
    void _OnSignalOpen(const std::vector<Glib::RefPtr<Gio::File>> &files,
        const Glib::ustring &stuff);

    //! \brief Starts the background worker threads
    virtual void _StartWorkerThreads();

    //! \brief Waits for worker threads to quit.
    //!
    //! Must hbe called before destructing, otherwise the background threads will
    //! assert
    virtual void _WaitForWorkerThreads();
    
private:

    Glib::RefPtr<Gtk::Application> Application;

    Glib::RefPtr<Gtk::Builder> MainBuilder;
    
    Gtk::Window* MainMenu = nullptr;
    Gtk::Window* WelcomeWindow = nullptr;


    std::shared_ptr<Collection> UncategorizedCollection;

    //! \brief If true everything that is created should be marked private. When not in
    //! private mode things that were marked private shouldn't be visible
    bool IsInPrivateMode = false;

    // Startup code //
    //! Makes sure initialization is ran only once
    bool IsInitialized = false;
    std::thread LoadThread;
    Glib::Dispatcher StartDispatcher;

    std::atomic<bool> LoadError = { false };
    std::atomic<bool> QuitWorkerThreads = { false };

    //! Set to true once _OnLoadingFinished is done
    std::atomic<bool> LoadCompletelyFinished = { false };

    //! Used to call the main thread when a message has been added
    Glib::Dispatcher MessageDispatcher;

    //! Must be locked when any of the message queues is changed
    std::mutex MessageQueueMutex;

    //! When windows have closed or they want to be closed they send
    //! an event here through DualView::WindowClosed
    //! MessageQueueMutex must be locked when changing this
    std::list<std::shared_ptr<WindowClosedEvent>> CloseEvents;

    //! Command line commands are stored here while the application
    //! is loading
    std::list<std::function<void (DualView&)>> QueuedCmds;

    //! Mutex for accessing QueuedCmds
    std::mutex QueuedCmdsMutex;

    //! Set to true to suppress destructor notify message (to the main instance)
    //! if not initialized
    bool SuppressSecondInstance = false;

    //! List of open windows
    //! Used to keep the windows allocated while they are open
    std::vector<std::shared_ptr<BaseWindow>> OpenWindows;

    //! Collection window
    std::shared_ptr<CollectionView> _CollectionView;

    //! Plugin manager. For loading extra functionality
    std::unique_ptr<PluginManager> _PluginManager;

    //! CacheManager handles loading all images
    std::unique_ptr<CacheManager> _CacheManager;

    //! Database holds all the data related to images
    std::unique_ptr<Database> _Database;

    //! Logger object
    std::unique_ptr<Leviathan::Logger> _Logger;

    //! Main settings
    std::unique_ptr<Settings> _Settings;

    //! A wrapper for the global curl instance
    std::unique_ptr<CurlWrapper> _CurlWrapper;

    //! Hash loading thread
    std::thread HashCalculationThread;
    std::condition_variable HashCalculationThreadNotify;

    std::list<std::weak_ptr<Image>> HashImageQueue;
    std::mutex HashImageQueueMutex;


    //! Database background thread
    std::thread DatabaseThread;
    std::condition_variable DatabaseThreadNotify;

    std::list<std::unique_ptr<std::function<void()>>> DatabaseFuncQueue;
    std::mutex DatabaseFuncQueueMutex;


    static DualView* Staticinstance;
};


}

