#pragma once
#include <gtkmm.h>

#include "windows/BaseWindow.h"

#include "Common.h"

#include "VirtualPath.h"

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
class Folder;
class AppliedTag;
class Tag;
class TagModifier;
class ImageListScroll;

class PluginManager;
class CacheManager;
class CurlWrapper;
class Database;
class DownloadManager;
class ChangeEvents;

class Settings;

class CollectionView;
class TagManager;
class Downloader;
class DownloadSetup;
class DebugWindow;

struct ResolvePathInfinityBlocker;

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

    //! \brief Opens an image viewer for a file
    void OpenImageViewer(std::shared_ptr<Image> image,
        std::shared_ptr<ImageListScroll> scroll);

    //! \brief Opens Collection viewer for collection
    void OpenSingleCollectionView(std::shared_ptr<Collection> collection);

    //! \brief Opens a window that allows adding a Collection to a folder
    void OpenAddToFolder(std::shared_ptr<Collection> collection);

    //! \brief Opens a window that allows removing a Collection from folder(s)
    void OpenRemoveFromFolders(std::shared_ptr<Collection> collection);

    //! \brief Opens the tag creation window with the text already filled in
    void OpenTagCreator(const std::string &settext);

    //! \brief Opens tag info window with the specified tag selected / as the search text
    void OpenTagInfo(const std::string &tagtext);

    //! \brief Opens the tag creation window
    void OpenTagCreator();

    //! \brief Opens an empty Importer
    void OpenImporter();

    //! \brief Opens an Importer with specified database images
    void OpenImporter(const std::vector<std::shared_ptr<Image>> &images);

    //! \brief Opens the Downloader
    void OpenDownloader();

    //! \brief Opens a setup window for a new downloadable gallery
    //! \param useropened True when the user clicked something and this was opened. If this is
    //! automatically opened for some other type of action then this is false
    void OpenDownloadSetup(bool useropened = true);

    //! \brief Opens a download setup for a link that was sent to us from another program
    void OnNewImageLinkReceived(const std::string &url, const std::string &referrer);

    //! \brief Opens a download setup for a gallery that was sent to us from another program
    void OnNewGalleryLinkReceived(const std::string &url);
    

    //! \brief Runs folder creator as a modal window
    void RunFolderCreatorAsDialog(const VirtualPath &path, const std::string &prefillnewname,
        Gtk::Window &parentwindow);

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

    //! \brief Queues a function to be ran on a worker thread
    void QueueWorkerFunction(std::function<void()> func);

    //! \brief Queues a function to be ran on a worker thread repeatedly until it returns true
    //! indicating that it has succeeded
    void QueueConditional(std::function<bool()> func);

    //! \brief Queues a function to be ran on the main thread
    void InvokeFunction(std::function<void()> func);

    //! \brief Makes sure function is ran on the main thread.
    //!
    //! Either by queueing it or running it immediately if the main thread calls this function
    void RunOnMainThread(const std::function<void()> &func);

    //! \brief Returns a path to the collection root folder, where imported images are
    //! copied or moved to
    std::string GetPathToCollection(bool isprivate) const;

    //! \brief Makes a target file path shorter than DUALVIEW_MAX_ALLOWED_PATH and one
    //! that doesn't exist
    static std::string MakePathUniqueAndShort(const std::string &path);

    //! \brief Calculates a sha hash of a string and base64 encodes it
    //!
    //! Also any characthers not valid in paths will be replaced
    static std::string CalculateBase64EncodedHash(const std::string &str);
    
    //! \brief Moves an image to the folder determined from the collection's name
    //! \return True if succeeded, false if it failed for some reason
    //! \param move If true the file will be moved. If false the file will be copied instead
    bool MoveFileToCollectionFolder(std::shared_ptr<Image> img,
        std::shared_ptr<Collection> collection, bool move);

    //! \brief Function for moving files.
    //!
    //! This is used because boost::filesystem::rename doesn't work for files on different
    //! hard drives
    //! \note targetname will be overwritten if it exists already
    //! \returns False if cannot move
    //! \exception boost::filesystem::filesystem_error When something is badly wrong
    static bool MoveFile(const std::string &original, const std::string &targetname);

    //! \brief Returns true if string is in SUPPORTED_EXTENSIONS
    static bool IsExtensionContent(const std::string &extension);
    
    //! \brief Returns true if file extension is in SUPPORTED_EXTENSIONS
    static bool IsFileContent(const std::string &file);

    //! \brief Converts a unicode string to lower
    //! \todo Cache the boost::locale::generator
    static std::string StringToLower(const std::string &str);
    
    //
    // Database insert and modify functions
    //
    
    //! \brief Imports images to the database and adds them to the collection
    //! \param move If true the original file is deleted (only if  the file is not in the
    //! collection folder)
    bool AddToCollection(std::vector<std::shared_ptr<Image>> resources, bool move,
        std::string collectionname, const TagCollection &addcollectiontags,
        std::function<void(float)> progresscallback = nullptr);

    //! \brief Removes an Image from Collection and makes sure it is at least in Uncategorized
    void RemoveImageFromCollection(std::shared_ptr<Image> resource,
        std::shared_ptr<Collection> collection);

    //! \brief Retrieves a Collection from the database by name
    std::shared_ptr<Collection> GetOrCreateCollection(const std::string &name, bool isprivate);

    //! \brief Adds a Collection to a Folder
    //! \note If added to any other folder than root and removefromroot is true
    //! the folder is removed from the root folder
    void AddCollectionToFolder(std::shared_ptr<Folder> folder,
        std::shared_ptr<Collection> collection, bool removefromroot = true);

    //! \brief Removes a Collection from a folder
    //!
    //! If the Collection is no longer in any folder it will be added to root
    void RemoveCollectionFromFolder(std::shared_ptr<Collection> collection,
        std::shared_ptr<Folder> folder);
        
    
    //
    // Database object retrieve functions
    //
    std::shared_ptr<Folder> GetRootFolder();

    std::shared_ptr<Collection> GetUncategorized();

    //! \brief Returns paths for folders which contain collection
    std::vector<std::string> GetFoldersCollectionIsIn(std::shared_ptr<Collection> collection);

    //! \brief Retrieves a Folder from path
    //! \returns Null if the folder doesn't exist
    //! \todo Fix " and ' in the path
    std::shared_ptr<Folder> GetFolderFromPath(const VirtualPath &path);

    //! \brief Returns the first viable path to folder with id
    //! \note This cannot always properly handle circular parent folders
    VirtualPath ResolvePathToFolder(DBID id);

    //! \brief Parses an AppliedTag from a string. Doesn't add it to the database automatically
    //! \note This will lock the database, so if it already locked this causes a deadlock
    std::shared_ptr<AppliedTag> ParseTagFromString(std::string str) const;


    //! \brief Parses a tag and returns a list of suggestions
    //!
    //! If the str is a valid tag the first suggestion == str
    std::vector<std::string> GetSuggestionsForTag(std::string str, size_t maxcount = 100)
        const;

    //! \brief Retrieves tag names, modifiers, aliases, super aliases, and common modifiers
    //! containing str
    void RetrieveTagsMatching(std::vector<std::string> &result, const std::string &str) const;


    

    //! \brief Helper for ParseTagFromString
    //!
    //! Parses tag that matches a break rule
    //! \todo Select all possibly working rules from the database (currently selects only one)
    std::shared_ptr<AppliedTag> ParseTagWithBreakRule(const std::string &str) const;

    //! \brief Helper for ParseTagFromString
    //!
    //! \returns replacement text for str if it matches a super alias
    std::string GetExpandedTagFromSuperAlias(const std::string &str) const;

    //! \brief Helper for ParseTagFromString
    //!
    //! Parses a tag that is only a name, by either it being a tag, an alias, there being a
    //! matching break rule OR with a super alias (will throw if the super alias is invalid)
    std::shared_ptr<AppliedTag> ParseTagName(const std::string &str) const;

    //! \brief Helper for ParseTagFromString
    //!
    //! Parses tag of the form: "tag word tag"
    //! \note The first AppliedTag will have the CombinedWith member set
    std::tuple<std::shared_ptr<AppliedTag>, std::string, std::shared_ptr<AppliedTag>>
        ParseTagWithComposite(const std::string &str) const;
    
    //! \brief Helper for ParseTagFromString
    //!
    //! Parses tag of the form: "modifier modifier tag"
    std::shared_ptr<AppliedTag>
        ParseTagWithOnlyModifiers(const std::string &str) const;
        
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

    //! \brief Returns the logger object
    inline Leviathan::Logger* GetLogger() const{

        return _Logger.get();
    }

    inline DownloadManager& GetDownloadManager() const{
        
        return *_DownloadManager;
    }

    inline PluginManager& GetPluginManager() const{
        
        return *_PluginManager;
    }

    inline ChangeEvents& GetEvents() const{

        return *_ChangeEvents;
    }
    
    //! \brief Returns true if called on the main thread
    //!
    //! Used to detect errors where functions are called on the wrong thread
    static bool IsOnMainThread();

    //! \brief Asserts if IsOnMainThread returns false
    static void IsOnMainThreadAssert();
    
    //! \brief Returns the global instance or asserts and quits the program
    static DualView& Get();

protected:

    //! Helper for DualView::ParseTagWithOnlyModifiers
    std::shared_ptr<AppliedTag> ParseHelperCheckModifiersAndBreakRules(
        const std::shared_ptr<AppliedTag> &maintag, const std::vector<std::string*> &words,
        bool taglast) const;

    //! \brief Constructor for test subclass to use
    DualView(bool tests, const std::string &dbfile);

    //! \brief Constructor for test subclass to use
    DualView(bool tests, bool memorysettings);

    //! \brief Constructor for test subclass to use
    DualView(std::string tests, std::unique_ptr<Database> &&db = nullptr);

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
    //! also gtk parameter is needed to make sure that the program doesn't quit if there are
    //! other windows open.
    void _AddOpenWindow(std::shared_ptr<BaseWindow> window, Gtk::Window &gtk);

    //! \brief Handles all queued commandline arguments
    void _ProcessCmdQueue();

    //! \brief Processes hash calculation queue
    //! \todo Make this return images that are duplicates of currently loaded images,
    //! that are loaded but aren't in the database
    void _RunHashCalculateThread();

    //! \brief Processes DatabaseFuncQueue
    void _RunDatabaseThread();

    //! \brief Processer worker queue
    void _RunWorkerThread();

    //! \brief Conditional worker thread
    void _RunConditionalThread();

    std::tuple<bool, VirtualPath> ResolvePathHelperRecursive(DBID currentid,
        const VirtualPath &currentpath, const ResolvePathInfinityBlocker &earlieritems);
    
private:

    // Gtk callbacks
    void OpenImageFile_OnClick();

    void OpenCollection_OnClick();

    void OpenDebug_OnClick();

    //! Once an instance is loaded init can start properly
    void _OnInstanceLoaded();

    //! \brief Extra instances will pass parameters here
    //! \returns The exit code for the extra instance
    int _HandleCmdLine(const Glib::RefPtr<Gio::ApplicationCommandLine> &command_line);


    int _OnPreParseCommandLine(const Glib::RefPtr<Glib::VariantDict> &options);

    //! \brief Receives a list of files to open
    void _OnSignalOpen(const std::vector<Glib::RefPtr<Gio::File>> &files,
        const Glib::ustring &stuff);

    //! \brief Processes InvokeQueue
    void _ProcessInvokeQueue();

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

    //! The default collection for images
    //! \note Do not access directly, use the Get method for this. This may not be initialized
    //! if the Get method hasn't been called
    std::shared_ptr<Collection> UncategorizedCollection;
    
    //! The root folder. All collections must be in a folder that is a descendant of the root
    //! folder. Otherwise the collection is invisible
    //! \note Do not access directly, use the Get method for this. This may not be initialized
    //! if the Get method hasn't been called
    std::shared_ptr<Folder> RootFolder;

    //! \brief If true everything that is created should be marked private. When not in
    //! private mode things that were marked private shouldn't be visible
    bool IsInPrivateMode = false;

    // Startup code //
    //! Makes sure initialization is ran only once
    bool IsInitialized = false;
    std::thread LoadThread;
    //! Thread that loads the time zone database, this doesn't need to finish, because it locks
    //! a mutex that will block all time parsing while this is still loading
    std::thread DateInitThread;
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

    //! Emitted on when a worker thread wants to run something on the main thread
    Glib::Dispatcher InvokeDispatcher;

    //! Queued functions to run on the main thread
    std::list<std::function<void ()>> InvokeQueue;
    std::mutex InvokeQueueMutex;

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

    //! List of open download setups, used to pass things around between them
    //! and open received links in them
    //! \todo Clean up this list from time to time
    std::vector<std::weak_ptr<DownloadSetup>> OpenDLSetups;

    //! Collection window
    std::shared_ptr<CollectionView> _CollectionView;

    //! Tag editing window
    std::shared_ptr<TagManager> _TagManager;

    //! Downloader window
    std::shared_ptr<Downloader> _Downloader;

    //! Debug buttons window
    std::shared_ptr<DebugWindow> _DebugWindow;

    //! Plugin manager. For loading extra functionality
    std::unique_ptr<PluginManager> _PluginManager;

    //! CacheManager handles loading all images
    std::unique_ptr<CacheManager> _CacheManager;

    //! Database holds all the data related to images
    std::unique_ptr<Database> _Database;

    //! Events for database events that aren't related to actual objects
    std::unique_ptr<ChangeEvents> _ChangeEvents;

    //! Download manager drives all network related activity
    std::unique_ptr<DownloadManager> _DownloadManager;

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

    //! Worker threads
    std::thread Worker1Thread;
    
    std::condition_variable WorkerThreadNotify;
    std::list<std::unique_ptr<std::function<void()>>> WorkerFuncQueue;
    std::mutex WorkerFuncQueueMutex;

    //! Conditional worker
    std::thread ConditionalWorker1;

    std::condition_variable ConditionalWorkerThreadNotify;
    std::vector<std::shared_ptr<std::function<bool()>>> ConditionalFuncQueue;
    std::mutex ConditionalFuncQueueMutex;


    static DualView* Staticinstance;
};


}

