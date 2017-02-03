// ------------------------------------ //
#include "DualView.h"
#include "Common.h"


#include "windows/SingleView.h"
#include "windows/CollectionView.h"
#include "windows/Importer.h"
#include "windows/TagManager.h"
#include "windows/FolderCreator.h"
#include "windows/SingleCollection.h"
#include "windows/Downloader.h"
#include "windows/DownloadSetup.h"
#include "windows/AddToFolder.h"
#include "windows/RemoveFromFolders.h"
#include "windows/DebugWindow.h"

#include "core/CacheManager.h"
#include "core/Database.h"
#include "core/ChangeEvents.h"
#include "core/CurlWrapper.h"

#include "resources/Collection.h"
#include "resources/Image.h"
#include "resources/Tags.h"
#include "resources/Folder.h"

#include "Settings.h"
#include "PluginManager.h"
#include "DownloadManager.h"
#include "Exceptions.h"

#include "UtilityHelpers.h"

#include "Common/StringOperations.h"
#include "Iterators/StringIterator.h"

#include <iostream>
#include <chrono>
#include <boost/filesystem.hpp>

#include <cryptopp/sha.h>


#include "third_party/base64.h"

using namespace DV;
// ------------------------------------ //

//! Used for thread detection
thread_local static int32_t ThreadSpecifier = 0;

//! Asserts if not called on the main thread
inline void AssertIfNotMainThread(){

    LEVIATHAN_ASSERT(DualView::IsOnMainThread(), "Function called on the wrong thread");
}

DualView::DualView(Glib::RefPtr<Gtk::Application> app) : Application(app){

    Staticinstance = this;
    ThreadSpecifier = MAIN_THREAD_MAGIC;

    // Listen for open events //
    app->signal_activate().connect(sigc::mem_fun(*this, &DualView::_OnInstanceLoaded));
    app->signal_command_line().connect(sigc::mem_fun(*this, &DualView::_HandleCmdLine),
        false);

    // This is called when the application is ran with a file //
    app->signal_open().connect(sigc::mem_fun(*this, &DualView::_OnSignalOpen));

    app->signal_handle_local_options().connect(sigc::mem_fun(*this,
            &DualView::_OnPreParseCommandLine));
}

DualView::~DualView(){

    QuitWorkerThreads = true;

    if(_CacheManager){

        // Cache manager start close //
        _CacheManager->QuitProcessingThreads();
    }

    // Signal downloads to end after the next one
    if(_DownloadManager)
        _DownloadManager->StopDownloads();

    if(!IsInitialized){

        _WaitForWorkerThreads();
        _CacheManager.reset();
        _Settings.reset();

        // Reset staticinstance only if it is still us. This is for running tests
        if(Staticinstance == this)
            Staticinstance = nullptr;
        
        if(!SuppressSecondInstance)
            std::cout << "DualView++ Main Instance Notified (If there were no errors). \n"
                "Extra instance quitting" << std::endl;
        
        return;
    }

    LOG_INFO("DualView releasing resources");

    // Force close windows //
    OpenWindows.clear();

    _CollectionView.reset();
    _TagManager.reset();
    _Downloader.reset();
    _DebugWindow.reset();

    // Downloads should have already stopped so this should be quick to delete //
    _DownloadManager.reset();

    // Unload plugins //
    _PluginManager.reset();

    // Close windows managed directly by us //
    WelcomeWindow->close();
    delete WelcomeWindow;
    WelcomeWindow = nullptr;
    
    MainMenu->close();
    delete MainMenu;
    MainMenu = nullptr;

    // Unload image loader. All images must be closed before this is called //
    _CacheManager.reset();

    _Settings.reset();

    _WaitForWorkerThreads();

    // Let go of last database resources //
    UncategorizedCollection.reset();
    RootFolder.reset();
    
    // Close database //
    _Database.reset();

    _ChangeEvents.reset();

    Staticinstance = nullptr;
}

DualView::DualView(bool tests, const std::string &dbfile){

    _Logger = std::make_unique<Leviathan::Logger>("test_log.txt");

    LEVIATHAN_ASSERT(tests, "DualView test constructor called with false");

    SuppressSecondInstance = true;

    Staticinstance = this;
    ThreadSpecifier = MAIN_THREAD_MAGIC;

    _CacheManager = std::make_unique<CacheManager>();

    _Settings = std::make_unique<Settings>("test_settings.levof");

    _ChangeEvents = std::make_unique<ChangeEvents>();

    if(!dbfile.empty())
        _Database = std::make_unique<Database>(dbfile);

    _StartWorkerThreads();
}

DualView::DualView(bool tests, bool memorysettings){

    _Logger = std::make_unique<Leviathan::Logger>("test_log_m.txt");

    LEVIATHAN_ASSERT(tests && memorysettings, "DualView test constructor called with false");

    SuppressSecondInstance = true;

    Staticinstance = this;
    ThreadSpecifier = MAIN_THREAD_MAGIC;

    _CacheManager = std::make_unique<CacheManager>();

    _Settings = std::make_unique<Settings>("memory");

    _ChangeEvents = std::make_unique<ChangeEvents>();
    
    _Database = std::make_unique<Database>(true);
    
    _StartWorkerThreads();
}

DualView::DualView(std::string tests, std::unique_ptr<Database> &&db /*= nullptr*/){

    _Logger = std::make_unique<Leviathan::Logger>("empty_dualview_log.txt");

    if(db)
        _Database = std::move(db);

    _ChangeEvents = std::make_unique<ChangeEvents>();

    LEVIATHAN_ASSERT(tests == "empty", "DualView test constructor called with not empty");

    SuppressSecondInstance = true;

    Staticinstance = this;
}


DualView* DualView::Staticinstance = nullptr;

DualView& DualView::Get(){

    LEVIATHAN_ASSERT(Staticinstance, "DualView static instance is null");
    return *Staticinstance;
}

bool DualView::IsOnMainThread(){

    return ThreadSpecifier == MAIN_THREAD_MAGIC;
}

void DualView::IsOnMainThreadAssert(){

    LEVIATHAN_ASSERT(DualView::IsOnMainThread(), "Function not called on the main thread");
}
// ------------------------------------ //
void DualView::_OnInstanceLoaded(){

    if(IsInitialized){

        return;
    }

    AssertIfNotMainThread();

    _Logger = std::make_unique<Leviathan::Logger>("log.txt");

    if(!_Logger){
        
        std::cerr << "Logger creation failed" << std::endl;
        throw Leviathan::InvalidState("Log opening failed");
        return;
    }
    
    LOG_WRITE("DualView++ Starting. Version " + std::string(DV::DUALVIEW_VERSION));

    // Create objects with simple constructors //
    _PluginManager = std::make_unique<PluginManager>();
    LEVIATHAN_ASSERT(_PluginManager, "Alloc failed in DualView constructor");
    
    // Connect dispatcher //
    StartDispatcher.connect(sigc::mem_fun(*this, &DualView::_OnLoadingFinished));
    MessageDispatcher.connect(sigc::mem_fun(*this, &DualView::_HandleMessages));
    InvokeDispatcher.connect(sigc::mem_fun(*this, &DualView::_ProcessInvokeQueue));
    

    MainBuilder = Gtk::Builder::create_from_file(
        "../gui/main_gui.glade");

    // Get all the glade resources //
    MainBuilder->get_widget("WelcomeWindow", WelcomeWindow);
    LEVIATHAN_ASSERT(WelcomeWindow, "Invalid .glade file");
    
    MainBuilder->get_widget("MainMenu", MainMenu);
    LEVIATHAN_ASSERT(MainMenu, "Invalid .glade file");

    // Show the loading window //
    Application->add_window(*WelcomeWindow);
    WelcomeWindow->show();

    // Database events are needed here for widgets to register themselves
    // and the load thread needs this when database Init is called
    _ChangeEvents = std::make_unique<ChangeEvents>();

    // Start loading thread //
    LoadThread = std::thread(std::bind(&DualView::_RunInitThread, this));

    // Get rest of the widgets while load thread is already running //
    Gtk::Button* OpenImageFile = nullptr;
    MainBuilder->get_widget("OpenImageFile", OpenImageFile);
    LEVIATHAN_ASSERT(OpenImageFile, "Invalid .glade file");

    OpenImageFile->signal_clicked().connect(
        sigc::mem_fun(*this, &DualView::OpenImageFile_OnClick));


    Gtk::Button* OpenCollection = nullptr;
    MainBuilder->get_widget("OpenCollection", OpenCollection);
    LEVIATHAN_ASSERT(OpenCollection, "Invalid .glade file");

    OpenCollection->signal_clicked().connect(
        sigc::mem_fun(*this, &DualView::OpenCollection_OnClick));


    Gtk::ToolButton* OpenDebug = nullptr;

    MainBuilder->get_widget("OpenDebug", OpenDebug);
    LEVIATHAN_ASSERT(OpenDebug, "Invalid .glade file");

    OpenDebug->signal_clicked().connect(
        sigc::mem_fun(*this, &DualView::OpenDebug_OnClick));

    Gtk::Button* OpenImporter = nullptr;
    MainBuilder->get_widget("OpenImporter", OpenImporter);
    LEVIATHAN_ASSERT(OpenImporter, "Invalid .glade file");

    OpenImporter->signal_clicked().connect(
        sigc::mem_fun<void>(*this, &DualView::OpenImporter));

    Gtk::Button* OpenTags = nullptr;
    MainBuilder->get_widget("OpenTags", OpenTags);
    LEVIATHAN_ASSERT(OpenTags, "Invalid .glade file");

    OpenTags->signal_clicked().connect(
        sigc::mem_fun<void>(*this, &DualView::OpenTagCreator));

    Gtk::Button* OpenDownloader = nullptr;
    MainBuilder->get_widget("OpenDownloader", OpenDownloader);
    LEVIATHAN_ASSERT(OpenDownloader, "Invalid .glade file");

    OpenDownloader->signal_clicked().connect(
        sigc::mem_fun<void>(*this, &DualView::OpenDownloader));
    

    //_CollectionView
    CollectionView* tmpCollection = nullptr;
    MainBuilder->get_widget_derived("CollectionView", tmpCollection);
    LEVIATHAN_ASSERT(tmpCollection, "Invalid .glade file");

    // Store the window //
    _CollectionView = std::shared_ptr<CollectionView>(tmpCollection);
    
    // TagManager
    TagManager* tmpTagManager = nullptr;
    MainBuilder->get_widget_derived("TagManager", tmpTagManager);
    LEVIATHAN_ASSERT(tmpTagManager, "Invalid .glade file");

    // Store the window //
    _TagManager = std::shared_ptr<TagManager>(tmpTagManager);

    // Downloader
    Downloader* tmpDownloader = nullptr;
    MainBuilder->get_widget_derived("Downloader", tmpDownloader);
    LEVIATHAN_ASSERT(tmpDownloader, "Invalid .glade file");

    // Store the window //
    _Downloader = std::shared_ptr<Downloader>(tmpDownloader);


    //_CollectionView
    DebugWindow* tmpDebug = nullptr;
    MainBuilder->get_widget_derived("DebugWindow", tmpDebug);
    LEVIATHAN_ASSERT(tmpDebug, "Invalid .glade file");

    // Store the window //
    _DebugWindow = std::shared_ptr<DebugWindow>(tmpDebug);
    

    // Initialize accelerator keys here
    //Gtk::AccelMap::add_entry("<CollectionList-Item>/Right/Help", GDK_KEY_H,
    //    Gdk::CONTROL_MASK);

    // Start worker threads //
    _StartWorkerThreads();

    LOG_INFO("Basic initialization completed");
    
    IsInitialized = true;
}

bool DualView::_DoInitThreadAction(){

    // Load settings //
    try{
        _Settings = std::make_unique<Settings>("dv_settings.levof");
        
    } catch(const Leviathan::InvalidArgument &e){

        LOG_ERROR("Invalid configuration. Please delete it and try again:");
        e.PrintToLog();
        return true;
    }

    _Settings->VerifyFoldersExist();

    // Load curl //
    _CurlWrapper = std::make_unique<CurlWrapper>();

    // Load plugins //
    const auto plugins = _Settings->GetPluginList();
    
    if(!plugins.empty()){

        const auto pluginFolder = boost::filesystem::path(_Settings->GetPluginFolder());

        LOG_INFO("Loading " + Convert::ToString(plugins.size()) + " plugin(s)");

        for(const auto& plugin : plugins){

            // Plugin name
        #ifdef _WIN32
            const auto libname = plugin + ".dll";
        #else
            const auto libname = "lib" + plugin + ".so";
        #endif //_WIN32

            if(!_PluginManager->LoadPlugin((pluginFolder /  libname).string())){

                LOG_ERROR("Failed to load plugin: " + plugin);
                return true;
            } 
        }

        // Print how many plugins are loaded //
        _PluginManager->PrintPluginStats();
    }

    // Start downloader threads and load more curl instances
    _DownloadManager = std::make_unique<DownloadManager>();

    // Load ImageMagick library //
    _CacheManager = std::make_unique<CacheManager>();

    // Load database //    
    // Database object
    _Database = std::make_unique<Database>(_Settings->GetDatabaseFile());

    try{
        _Database->Init();

    } catch(const Leviathan::InvalidState &e){

        LOG_ERROR("Database initialization failed: ");
        e.PrintToLog();
        return true;
    } catch(const InvalidSQL &e){

        LOG_ERROR("Database initialization logic has a bug, sql error: ");
        e.PrintToLog();
        return true;
    }

    DatabaseThread = std::thread(&DualView::_RunDatabaseThread, this);

    // Succeeded //
    return false;
}

void DualView::_RunInitThread(){

    LOG_INFO("Running Init thread");
    LoadError = false;

    DateInitThread = std::thread([]() -> void {

            // Load timezone database
            TimeHelpers::TimeZoneDatabaseSetup();
        });

    bool result = _DoInitThreadAction();
    
    if(result){
        // Sleep if an error occured to let the user read it
        LoadError = true;
        
        //std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    }

    // Invoke the callback on the main thread //
    StartDispatcher.emit();
}

void DualView::_OnLoadingFinished(){

    AssertIfNotMainThread();

    // The thread needs to be joined or an exception is thrown
    if(LoadThread.joinable())
        LoadThread.join();

    if(LoadError){

        // Loading failed
        LOG_ERROR("Loading Failed");
        WelcomeWindow->close();
        return;
    }

    LOG_INFO("Loading Succeeded");

    Application->add_window(*MainMenu);
    MainMenu->show();

    // Hide the loading window after, just in case
    WelcomeWindow->close();

    // Run command line //
    LoadCompletelyFinished = true;

    _ProcessInvokeQueue();
    
    // For testing SuperViewer uncomment this to quickly open images
    //OpenImageViewer("/home/hhyyrylainen/690806.jpg");
    //OpenImporter();



}
// ------------------------------------ //
int DualView::_HandleCmdLine(const Glib::RefPtr<Gio::ApplicationCommandLine>
    &command_line)
{

    // First handle already parsed things //
    auto alreadyParsed = command_line->get_options_dict();

    Glib::ustring referrer;
    if(!alreadyParsed->lookup_value("dl-referrer", referrer)){

        referrer = "";
    }
    

    Glib::ustring fileUrl;
    if(alreadyParsed->lookup_value("dl-image", fileUrl)){

        InvokeFunction([=]() -> void
                {
                    LOG_INFO("File to download: " + std::string(fileUrl.c_str()));
                    OnNewImageLinkReceived(fileUrl, referrer);
                });
    }

    if(alreadyParsed->lookup_value("dl-page", fileUrl)){

        InvokeFunction([=]() -> void
            {
                LOG_INFO("Page to download: " + std::string(fileUrl.c_str()));
                OnNewGalleryLinkReceived(fileUrl);
            });
    }

    if(alreadyParsed->lookup_value("dl-auto", fileUrl)){

        InvokeFunction([=]() -> void
            {
                LOG_INFO("Auto detect and download: " + std::string(fileUrl.c_str()));

                // If the file has a content extension then open as an image
                const auto extension = Leviathan::StringOperations::GetExtension(
                    DownloadManager::ExtractFileName(fileUrl.c_str()));
                
                if(IsExtensionContent("." + extension)){

                    LOG_INFO("Detected as an image");
                    OnNewImageLinkReceived(fileUrl, referrer);
                    return;
                }

                // Otherwise open as a page
                LOG_INFO("Detected as a gallery page");
                OnNewGalleryLinkReceived(fileUrl);
            });
    }

    int argc;
    char** argv = command_line->get_arguments(argc);

    if(argc > 1){

        std::cout << "Extra arguments: " << std::endl;

        for(int i = 1; i < argc; ++i)
            std::cout << argv[i] << (i + 1 < argc ? ", " : " ");

        std::cout << std::endl;

        std::cout << "TODO: check if it is a file and open" << std::endl;
    }

    g_strfreev(argv);
    
    Application->activate();

    return 0;
}

int DualView::_OnPreParseCommandLine(const Glib::RefPtr<Glib::VariantDict> &options){

    bool version;
    if(options->lookup_value("version", version)){

        std::cout << "DualView++ Version " << DUALVIEW_VERSION << std::endl;
        SuppressSecondInstance = true;
        return 0;
    }
    
    return -1;
}
// ------------------------------------ //
void DualView::_OnSignalOpen(const std::vector<Glib::RefPtr<Gio::File>> &files,
    const Glib::ustring &stuff)
{
    LOG_INFO("Got file list to open:");

    for(const auto& file : files){

        LOG_WRITE("\t" + file->get_path());
    }
}
// ------------------------------------ //
void DualView::_HandleMessages(){

    AssertIfNotMainThread();

    std::lock_guard<std::mutex> lock(MessageQueueMutex);

    // Handle all messages, because we might not get a dispatch for each message
    while(!CloseEvents.empty()){

        auto event = CloseEvents.front();
        CloseEvents.pop_front();
        
        // Handle the event
        // Find the window //
        for(auto iter = OpenWindows.begin(); iter != OpenWindows.end(); ++iter){

            if((*iter).get() == event->AffectedWindow){

                LOG_INFO("DualView: notified of a closed window");
                OpenWindows.erase(iter);
                break;
            }
        }
    }
}
// ------------------------------------ //
void DualView::QueueImageHashCalculate(std::shared_ptr<Image> img){

    std::lock_guard<std::mutex> lock(HashImageQueueMutex);

    HashImageQueue.push_back(img);

    HashCalculationThreadNotify.notify_all();
}

void DualView::_RunHashCalculateThread(){

    std::unique_lock<std::mutex> lock(HashImageQueueMutex);

    while(!QuitWorkerThreads){

        if(HashImageQueue.empty())
            HashCalculationThreadNotify.wait(lock);

        while(!HashImageQueue.empty()){

            auto img = HashImageQueue.front().lock();

            HashImageQueue.pop_front();

            if(!img){

                // Image has been deallocated already //
                continue;
            }

            lock.unlock();

            img->_DoHashCalculation();

            lock.lock();

            // Replace with an existing image if the hash exists //
            try{
                auto existing = _Database->SelectImageByHashAG(img->GetHash());

                if(existing){

                    LOG_INFO("Calculated hash for a duplicate image");
                    img->BecomeDuplicateOf(*existing);
                    continue;
                }
                
            } catch(const InvalidSQL&){

                // Database probably isn't initialized
            }
            
            img->_OnFinishHash();
        }
    }
}
// ------------------------------------ //
void DualView::QueueDBThreadFunction(std::function<void()> func){

    std::lock_guard<std::mutex> lock(DatabaseFuncQueueMutex);

    DatabaseFuncQueue.push_back(std::make_unique<std::function<void()>>(func));

    DatabaseThreadNotify.notify_all();
}

void DualView::QueueWorkerFunction(std::function<void()> func){

    std::lock_guard<std::mutex> lock(WorkerFuncQueueMutex);

    WorkerFuncQueue.push_back(std::make_unique<std::function<void()>>(func));

    WorkerThreadNotify.notify_one();
}

void DualView::QueueConditional(std::function<bool()> func){

    std::lock_guard<std::mutex> lock(ConditionalFuncQueueMutex);

    ConditionalFuncQueue.push_back(std::make_shared<std::function<bool()>>(func));

    ConditionalWorkerThreadNotify.notify_one();
}

void DualView::_RunDatabaseThread(){

    std::unique_lock<std::mutex> lock(DatabaseFuncQueueMutex);

    while(!QuitWorkerThreads){

        if(DatabaseFuncQueue.empty())
            DatabaseThreadNotify.wait(lock);

        while(!DatabaseFuncQueue.empty()){

            auto func = std::move(DatabaseFuncQueue.front());

            DatabaseFuncQueue.pop_front();

            lock.unlock();

            func-> operator()();
            
            lock.lock();
        }
    }
}

void DualView::_RunWorkerThread(){

    std::unique_lock<std::mutex> lock(WorkerFuncQueueMutex);

    while(!QuitWorkerThreads){

        if(WorkerFuncQueue.empty())
            WorkerThreadNotify.wait(lock);

        while(!WorkerFuncQueue.empty()){

            auto func = std::move(WorkerFuncQueue.front());

            WorkerFuncQueue.pop_front();

            lock.unlock();

            func-> operator()();
            
            lock.lock();
        }
    }
}

void DualView::_RunConditionalThread(){

    std::unique_lock<std::mutex> lock(ConditionalFuncQueueMutex);

    while(!QuitWorkerThreads){

        if(ConditionalFuncQueue.empty())
            ConditionalWorkerThreadNotify.wait(lock);

        for(size_t i = 0; i < ConditionalFuncQueue.size(); ){
                
            const auto func = ConditionalFuncQueue[i];

            lock.unlock();

            const auto result = func->operator ()();

            lock.lock();

            if(result){

                // Remove //
                ConditionalFuncQueue.erase(ConditionalFuncQueue.begin() + i);
                    
            } else {

                ++i;
            }
        }

        // Don't constantly check whether functions are ready to run
        if(!ConditionalFuncQueue.empty() && !QuitWorkerThreads)
            ConditionalWorkerThreadNotify.wait_for(lock, std::chrono::milliseconds(12));
    }
}
// ------------------------------------ //
void DualView::InvokeFunction(std::function<void()> func){

    std::unique_lock<std::mutex> lock(InvokeQueueMutex);

    InvokeQueue.push_back(func);

    // Notify main thread
    InvokeDispatcher.emit();
}

void DualView::RunOnMainThread(const std::function<void()> &func){

    if(IsOnMainThread()){

        func();
        
    } else {

        InvokeFunction(func);
    }
}

void DualView::_ProcessInvokeQueue(){

    // Wait until completely loaded before invoking
    if(!LoadCompletelyFinished)
        return;

    std::unique_lock<std::mutex> lock(InvokeQueueMutex);

    while(!InvokeQueue.empty()){

        const auto func = InvokeQueue.front();

        InvokeQueue.pop_front();

        lock.unlock();

        func();

        lock.lock();
    }
}
// ------------------------------------ //
void DualView::_StartWorkerThreads(){

    QuitWorkerThreads = false;

    HashCalculationThread = std::thread(std::bind(&DualView::_RunHashCalculateThread, this));
    Worker1Thread = std::thread(std::bind(&DualView::_RunWorkerThread, this));
    ConditionalWorker1 = std::thread(std::bind(&DualView::_RunConditionalThread, this));
}

void DualView::_WaitForWorkerThreads(){

    // Make sure this is set //
    QuitWorkerThreads = true;

    HashCalculationThreadNotify.notify_all();
    DatabaseThreadNotify.notify_all();
    WorkerThreadNotify.notify_all();
    ConditionalWorkerThreadNotify.notify_all();

    if(HashCalculationThread.joinable())
        HashCalculationThread.join();

    if(DatabaseThread.joinable())
        DatabaseThread.join();

    if(DateInitThread.joinable())
        DateInitThread.join();

    if(Worker1Thread.joinable())
        Worker1Thread.join();

    if(ConditionalWorker1.joinable())
        ConditionalWorker1.join();
}
// ------------------------------------ //
std::string DualView::GetPathToCollection(bool isprivate) const{

    if(isprivate){

        return _Settings->GetPrivateCollection();
        
    } else {

        return _Settings->GetPublicCollection();
    }
}

bool DualView::MoveFileToCollectionFolder(std::shared_ptr<Image> img,
    std::shared_ptr<Collection> collection, bool move)
{
    std::string targetfolder = "";

    // Special case, uncategorized //
    if(collection->GetID() == DATABASE_UNCATEGORIZED_COLLECTION_ID ||
        collection->GetID() == DATABASE_UNCATEGORIZED_PRIVATECOLLECTION_ID)
    {
        targetfolder = (boost::filesystem::path(
                GetPathToCollection(collection->GetIsPrivate())) / "no_category/").c_str();
    }
    else
    {
        targetfolder = (boost::filesystem::path(
                GetPathToCollection(collection->GetIsPrivate())) / "collections" /
            collection->GetNameForFolder()).c_str();
        
        boost::filesystem::create_directories(targetfolder);
    }

    // Skip if already there //
    if(boost::filesystem::equivalent(targetfolder,
            boost::filesystem::path(img->GetResourcePath()).remove_filename()))
    {
        return true;
    }

    const auto targetPath = boost::filesystem::path(targetfolder) /
        boost::filesystem::path(img->GetResourcePath()).filename();

    // Make short enough and unique //
    const auto finalPath = MakePathUniqueAndShort(targetPath.string());

    try{
        if(move){

            if(!MoveFile(img->GetResourcePath(), finalPath)){

                LOG_ERROR("Failed to move file to collection: " + img->GetResourcePath()
                    + " -> " + finalPath);
                return false;
            }
        
        } else {

            boost::filesystem::copy_file(img->GetResourcePath(), finalPath);
        }
    } catch(const boost::filesystem::filesystem_error &e){

        LOG_ERROR("Failed to copy file to collection: " + img->GetResourcePath()
            + " -> " + finalPath);
        LOG_WRITE("Exception: " + std::string(e.what()));
        return false;
    }

    LEVIATHAN_ASSERT(boost::filesystem::exists(finalPath),
        "Move to collection, final path doesn't exist after copy");
    
    // Notify image cache that the file was moved //
    if(move)
        _CacheManager->NotifyMovedFile(img->GetResourcePath(), finalPath);

    img->SetResourcePath(finalPath);
    return true;
}

bool DualView::MoveFile(const std::string &original, const std::string &targetname){

    // First try renaming //
    try{
        boost::filesystem::rename(original, targetname);

        // Renaming succeeded //
        return true;

    } catch(const boost::filesystem::filesystem_error &){

    }

    // Rename failed, we need to copy the file and delete the original //
    try{

        boost::filesystem::copy_file(original, targetname);

        // Make sure copy worked before deleting original //
        if(boost::filesystem::file_size(original) != boost::filesystem::file_size(targetname)){

            LOG_ERROR("File copy: new file is of different size");
            return false;
        }

        boost::filesystem::remove(original);
            
    } catch(const boost::filesystem::filesystem_error &){
        
        // Failed //
        throw;
    }
    
    return true;
}
// ------------------------------------ //
bool DualView::IsExtensionContent(const std::string &extension){

    for(const auto& type : SUPPORTED_EXTENSIONS){

        if(std::get<0>(type) == extension)
            return true;
    }

    return false;    
}

bool DualView::IsFileContent(const std::string &file){

    std::string extension = StringToLower(boost::filesystem::path(file).extension().string());
    
    return IsExtensionContent(extension);
}
// ------------------------------------ //
bool DualView::OpenImageViewer(const std::string &file){

    AssertIfNotMainThread();

    LOG_INFO("Opening single image for viewing: " + file);

    OpenImageViewer(Image::Create(file), nullptr);
    return true;
}

void DualView::OpenImageViewer(std::shared_ptr<Image> image,
    std::shared_ptr<ImageListScroll> scroll)
{
    AssertIfNotMainThread();

    auto builder = Gtk::Builder::create_from_file(
        "../gui/single_view.glade");

    SingleView* window;
    builder->get_widget_derived("SingleView", window);

    if(!window){

        LOG_ERROR("SingleView window GUI layout is invalid");
        return;
    }

    std::shared_ptr<SingleView> wrapped(window);
    _AddOpenWindow(wrapped, *window);
    wrapped->show();
    
    wrapped->Open(image, scroll);
}

void DualView::OpenSingleCollectionView(std::shared_ptr<Collection> collection){

    AssertIfNotMainThread();

    auto builder = Gtk::Builder::create_from_file(
        "../gui/single_collection.glade");

    SingleCollection* window;
    builder->get_widget_derived("SingleCollection", window);

    if(!window){

        LOG_ERROR("SingleCollection window GUI layout is invalid");
        return;
    }

    std::shared_ptr<SingleCollection> wrapped(window);
    _AddOpenWindow(wrapped, *window);
    wrapped->show();
    
    wrapped->ShowCollection(collection);
}

void DualView::OpenAddToFolder(std::shared_ptr<Collection> collection){

    AssertIfNotMainThread();

    auto window = std::make_shared<AddToFolder>(collection);
    _AddOpenWindow(window, *window);
    window->show();
}

void DualView::OpenRemoveFromFolders(std::shared_ptr<Collection> collection){
    
    AssertIfNotMainThread();

    auto window = std::make_shared<RemoveFromFolders>(collection);
    _AddOpenWindow(window, *window);
    window->show();
}

void DualView::OpenImporter(){

    AssertIfNotMainThread();

    auto builder = Gtk::Builder::create_from_file(
        "../gui/importer.glade");

    Importer* window;
    builder->get_widget_derived("FileImporter", window);

    if(!window){

        LOG_ERROR("Importer window GUI layout is invalid");
        return;
    }

    LOG_INFO("Opened Importer window");

    std::shared_ptr<Importer> wrapped(window);
    _AddOpenWindow(wrapped, *window);
    wrapped->show();
}

void DualView::OpenImporter(const std::vector<std::shared_ptr<Image>> &images){

    AssertIfNotMainThread();

    auto builder = Gtk::Builder::create_from_file(
        "../gui/importer.glade");

    Importer* window;
    builder->get_widget_derived("FileImporter", window);

    if(!window){

        LOG_ERROR("Importer window GUI layout is invalid");
        return;
    }

    window->AddExisting(images);

    LOG_INFO("Opened Importer window with images");

    std::shared_ptr<Importer> wrapped(window);
    _AddOpenWindow(wrapped, *window);
    wrapped->show();
}

void DualView::OpenDownloader(){

    AssertIfNotMainThread();

    // Show it //
    Application->add_window(*_Downloader);
    _Downloader->show();
    _Downloader->present();
}

void DualView::OpenTagCreator(const std::string &settext){

    OpenTagCreator();

    _TagManager->SetCreateTag(settext);
}

void DualView::OpenTagInfo(const std::string &tagtext){

    OpenTagCreator();

    _TagManager->SetSearchString(tagtext);
}

void DualView::OpenTagCreator(){

    AssertIfNotMainThread();

    // Show it //
    Application->add_window(*_TagManager);
    _TagManager->show();
    _TagManager->present();
}

void DualView::RunFolderCreatorAsDialog(const VirtualPath &path,
    const std::string &prefillnewname, Gtk::Window &parentwindow)
{
    AssertIfNotMainThread();

    bool isprivate = false;

    FolderCreator window(path, prefillnewname);

    window.set_transient_for(parentwindow);
    window.set_modal(true);

    int result = window.run();

    if(result != Gtk::RESPONSE_OK)
        return;

    std::string name;
    VirtualPath createpath;
    window.GetNewName(name, createpath);

    LOG_INFO("Trying to create new folder \"" + name + "\" at: " +
        createpath.operator std::string());

    try{
        auto parent = GetFolderFromPath(createpath);

        if(!parent)
            throw std::runtime_error("Parent folder at path doesn't exist");

        auto created = _Database->InsertFolder(name, isprivate, *parent);

        if(!created)
            throw std::runtime_error("Failed to create folder");
        
    } catch(const std::exception &e){

        auto dialog = Gtk::MessageDialog(parentwindow,
            "Failed to create folder \"" + name + "\" at: " +
            createpath.operator std::string(),
            false,
            Gtk::MESSAGE_ERROR,
            Gtk::BUTTONS_OK,
            true 
        );
        dialog.set_secondary_text("Try again without using special characters in the "
            "folder name. And verify that the path at which the folder is to be "
            "created is valid. Exception message: " + std::string(e.what()));
        dialog.run();
    }
}
// ------------------------------------ //
void DualView::OnNewImageLinkReceived(const std::string &url, const std::string &referrer){

    AssertIfNotMainThread();

    // Find a downloader that can be used
    std::shared_ptr<DownloadSetup> existing;
    bool openednew = false;

    while(true){
        for(const auto& dlsetup : OpenDLSetups){
        
            // Check is it good for us //
            auto locked = dlsetup.lock();

            if(!locked)
                continue;
        
            if(!locked->IsValidTargetForImageAdd())
                continue;

            // Found a good one //
            existing = locked;
            break;
        }

        if(existing){

            // We don't need a new window //
            existing->AddExternallyFoundLink(url, referrer);
            return;
        }

        if(openednew){

            LOG_ERROR("Failed to find a suitable DownloadSetup even after opening a new one");
            return;
        }

        openednew = true;
        
        // Open a new window and try again //
        OpenDownloadSetup(false);
    }
}

void DualView::OnNewGalleryLinkReceived(const std::string &url){

    AssertIfNotMainThread();

    auto scanner = _PluginManager->GetScannerForURL(url);

    if(scanner && scanner->IsUrlNotGallery(url)){

        LOG_INFO("New gallery link is actually a content page");
        OnNewImagePageLinkReceived(url);
        return;
    }

    // Find a downloader that can be used
    std::shared_ptr<DownloadSetup> existing;
    bool openednew = false;
    
    while(true){
    
        for(const auto& dlsetup : OpenDLSetups){
        
            // Check is it good for us //
            auto locked = dlsetup.lock();

            if(!locked)
                continue;
        
            if(!locked->IsValidForNewPageScan())
                continue;

            // Found a good one //
            existing = locked;
            break;
        }

        if(existing){

            // We don't need a new window //
            existing->SetNewUrlToDl(url);
            return;
        }

        if(openednew){

            LOG_ERROR("Failed to find a suitable DownloadSetup even after opening a new one");
            return;
        }

        openednew = true;
        
        // Open a new window and try again //
        OpenDownloadSetup(false);
    }
}

void DualView::OnNewImagePageLinkReceived(const std::string &url){

    // Find a downloader that can be used
    std::shared_ptr<DownloadSetup> existing;
    bool openednew = false;
    
    while(true){
    
        for(const auto& dlsetup : OpenDLSetups){
        
            // Check is it good for us //
            auto locked = dlsetup.lock();

            if(!locked)
                continue;
        
            if(!locked->IsValidTargetForScanLink())
                continue;

            // Found a good one //
            existing = locked;
            break;
        }

        if(existing){

            // We don't need a new window //
            existing->AddExternalScanLink(url);
            return;
        }

        if(openednew){

            LOG_ERROR("Failed to find a suitable DownloadSetup even after opening a new one");
            return;
        }

        openednew = true;
        
        // Open a new window and try again //
        OpenDownloadSetup(false);
    }
}

void DualView::OpenDownloadSetup(bool useropened /*= true*/){
    
    AssertIfNotMainThread();

    auto builder = Gtk::Builder::create_from_file(
        "../gui/download_setup.glade");

    DownloadSetup* window;
    builder->get_widget_derived("DownloadSetup", window);

    if(!window){

        LOG_ERROR("DownloadSetup window GUI layout is invalid");
        return;
    }

    LOG_INFO("Opened DownloadSetup window");

    std::shared_ptr<DownloadSetup> wrapped(window);
    _AddOpenWindow(wrapped, *window);
    OpenDLSetups.push_back(wrapped);

    if(useropened)
        wrapped->SetLockActive();
    
    wrapped->show();
}



// ------------------------------------ //
void DualView::RegisterWindow(Gtk::Window &window){

    Application->add_window(window);
}

void DualView::WindowClosed(std::shared_ptr<WindowClosedEvent> event){

    {
        std::lock_guard<std::mutex> lock(MessageQueueMutex);

        CloseEvents.push_back(event);
    }

    MessageDispatcher.emit();
}

void DualView::_AddOpenWindow(std::shared_ptr<BaseWindow> window, Gtk::Window &gtk){

    AssertIfNotMainThread();

    OpenWindows.push_back(window);
    RegisterWindow(gtk);
}
// ------------------------------------ //
std::string DualView::GetThumbnailFolder() const{

    return (boost::filesystem::path(GetSettings().GetPrivateCollection()) /
        boost::filesystem::path("thumbnails/")).c_str();
}
// ------------------------------------ //
// Database saving functions
bool DualView::AddToCollection(std::vector<std::shared_ptr<Image>> resources, bool move,
    std::string collectionname, const TagCollection &addcollectiontags,
    std::function<void(float)> progresscallback /*= nullptr*/)
{
    // Make sure ready to add //
    for(const auto& img : resources){

        if(!img->IsReady())
            return false;
    }

    // Trim whitespace //
    Leviathan::StringOperations::RemovePreceedingTrailingSpaces(collectionname);

    std::shared_ptr<Collection> addtocollection;

    bool canapplytags = true;

    auto uncategorized = GetUncategorized();

    // No collection specified, get Uncategorized //
    if(collectionname.empty()){
        
        addtocollection = uncategorized;
        canapplytags = false;

    } else {
        
        addtocollection = GetOrCreateCollection(collectionname, IsInPrivateMode);

        if (!addtocollection)
            throw Leviathan::InvalidArgument("Invalid collection name");
    }

    LEVIATHAN_ASSERT(addtocollection, "Failed to get collection object");
    
    if(canapplytags)
        addtocollection->AddTags(addcollectiontags);

    size_t currentitem = 0;
    const auto maxitems = resources.size();

    auto order = addtocollection->GetLastShowOrder();

    std::vector<std::string> filestodelete;

    // Save resources to database if not loaded from the database //

    for(auto& resource : resources){
        
        std::shared_ptr<Image> actualresource;

        if(!resource->IsInDatabase()){
            
            // If the image hash is in the collection then we shouldn't be here //
            // But just in case we should check to make absolutely sure
            auto existingimage = _Database->SelectImageByHashAG(resource->GetHash());

            if(existingimage){

                LOG_WARNING("Trying to import a duplicate hash image");

                // Delete original file if moving //
                if(move){

                    LOG_INFO("Deleting moved (duplicate) file: " +
                        resource->GetResourcePath());
                    
                    boost::filesystem::remove(resource->GetResourcePath());
                }
                
                if(resource->GetTags()->HasTags())
                    existingimage->GetTags()->AddTags(*resource->GetTags());

                actualresource = existingimage;
                
            } else {

                if (!MoveFileToCollectionFolder(resource, addtocollection, move)){
                
                    LOG_ERROR("Failed to move file to collection's folder");
                    return false;
                }
            
                std::shared_ptr<TagCollection> tagstoapply;

                // Store tags for applying //
                if(resource->GetTags()->HasTags())
                    tagstoapply = resource->GetTags();

                try{
                    _Database->InsertImage(*resource);
                
                } catch (const InvalidSQL &e){
                
                    // We have already moved the image so this is a problem
                    LOG_INFO("TODO: move file back after adding fails");
                    LOG_ERROR("Sql error adding image to collection: ");
                    e.PrintToLog();
                    return false;
                }

                // Apply tags //
                if(tagstoapply)
                    resource->GetTags()->AddTags(*tagstoapply);

                actualresource = resource;
            }
            
        } else {
            
            actualresource = resource;

            // Remove from uncategorized if not adding to that //
            if(addtocollection != uncategorized){
                
                uncategorized->RemoveImage(actualresource);
            }
        }

        LEVIATHAN_ASSERT(actualresource, "actualresource not set in DualView import image");

        // Associate with collection //
        addtocollection->AddImage(actualresource, ++order);

        currentitem++;

        if(progresscallback)
            progresscallback(currentitem / (float)maxitems);
    }

    // These are duplicate files of already existing ones
    bool exists = false;

    do{
        
        exists = false;

        for(const std::string& file : filestodelete){
            
            if(boost::filesystem::exists(file)){
                
                exists = true;

                try{
                    boost::filesystem::remove(file);
                } catch(const boost::filesystem::filesystem_error&){

                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

    } while(exists);

    return true;
}
// ------------------------------------ //
void DualView::RemoveImageFromCollection(std::shared_ptr<Image> resource,
    std::shared_ptr<Collection> collection)
{
    if(!resource || !collection)
        return;
    
    _Database->DeleteImageFromCollection(*collection, *resource);

    // Add to uncategorized //
    if(!_Database->SelectIsImageInAnyColllection(*resource)){

        LOG_INFO("Adding removed image to Uncategorized");
        GetUncategorized()->AddImage(resource);
    }
}
// ------------------------------------ //
std::shared_ptr<Collection> DualView::GetOrCreateCollection(const std::string &name,
    bool isprivate)
{
    auto existing = _Database->SelectCollectionByNameAG(name);

    if(existing)
        return existing;

    return _Database->InsertCollectionAG(name, isprivate);
}

void DualView::AddCollectionToFolder(std::shared_ptr<Folder> folder,
    std::shared_ptr<Collection> collection, bool removefromroot /*= true*/)
{
    if(!folder || !collection)
        return;

    _Database->InsertCollectionToFolderAG(*folder, *collection);

    if(!folder->IsRoot() && removefromroot){

        _Database->DeleteCollectionFromRootIfInAnotherFolder(*collection);
    }
    
}

void DualView::RemoveCollectionFromFolder(std::shared_ptr<Collection> collection,
    std::shared_ptr<Folder> folder)
{
    if(!folder || !collection)
        return;
    
    _Database->DeleteCollectionFromFolder(*folder, *collection);
    
    // Make sure the Collection is in at least one folder //
    _Database->InsertCollectionToRootIfInNone(*collection);
}

// ------------------------------------ //
// Database load functions
std::shared_ptr<Folder> DualView::GetRootFolder(){

    if(RootFolder)
        return RootFolder;

    LEVIATHAN_ASSERT(_Database, "Trying to GetRootFolder before database is opened");

    RootFolder = _Database->SelectRootFolderAG();
    return RootFolder;
}

std::shared_ptr<Collection> DualView::GetUncategorized(){

    if(UncategorizedCollection)
        return UncategorizedCollection;

    LEVIATHAN_ASSERT(_Database, "Trying to GetUncategorized before database is opened");

    UncategorizedCollection = _Database->SelectCollectionByNameAG("Uncategorized");
    return UncategorizedCollection;
}

// ------------------------------------ //
std::vector<std::string> DualView::GetFoldersCollectionIsIn(
    std::shared_ptr<Collection> collection)
{
    std::vector<std::string> result;

    if(!collection)
        return result;

    const auto folderids = _Database->SelectFoldersCollectionIsIn(*collection);

    for(auto id : folderids){

        result.push_back(ResolvePathToFolder(id));
    }

    return result;
}
// ------------------------------------ //
std::shared_ptr<Folder> DualView::GetFolderFromPath(const VirtualPath &path){

    // Root folder //
    if(path.IsRootPath())
        return GetRootFolder();


    // Loop through all the path components and verify that a folder exists

    std::shared_ptr<Folder> currentfolder;

    for(auto iter = path.begin(); iter != path.end(); ++iter){

        auto part = *iter;

        if(part.empty()){

            // String ended //
            return currentfolder;
        }

        if(!currentfolder && (part == "Root")){

            currentfolder = GetRootFolder();
            continue;
        }

        if(!currentfolder){

            // Didn't begin with root //
            return nullptr;
        }

        // Find a folder with the current name inside currentfolder //
        auto nextfolder = _Database->SelectFolderByNameAndParentAG(part, *currentfolder);

        if(!nextfolder){

            // There's a nonexistant folder in the path
            return nullptr;
        }

        // Moved to the next part
        currentfolder = nextfolder;
    }
    
    return currentfolder;
}

//! Prevents ResolvePathHelperRecursive from going through infinite loops
struct DV::ResolvePathInfinityBlocker{

    std::vector<DBID> EarlierFolders;
};

std::tuple<bool, VirtualPath> DualView::ResolvePathHelperRecursive(DBID currentid,
    const VirtualPath &currentpath, const ResolvePathInfinityBlocker &earlieritems)
{
    auto currentfolder = _Database->SelectFolderByIDAG(currentid);

    if(!currentfolder)
        return std::make_tuple(false, currentpath);

    if(currentfolder->IsRoot()){

        return std::make_tuple(true, VirtualPath() / currentpath);
    }

    ResolvePathInfinityBlocker infinity;
    infinity.EarlierFolders = earlieritems.EarlierFolders;
    infinity.EarlierFolders.push_back(currentid);
    
    for(auto parentid : _Database->SelectFolderParents(*currentfolder)){

        // Skip already seen folders //
        bool found = false;

        for(auto alreadychecked : infinity.EarlierFolders){

            if(alreadychecked == parentid){

                found = true;
                break;
            }
        }

        if(found)
            continue;
        
        const auto result = ResolvePathHelperRecursive(parentid,
            VirtualPath(currentfolder->GetName()) / currentpath, infinity);

        if(std::get<0>(result)){

            // Found a full path //
            return result;
        }
    }
    
    // Didn't find a full path //
    return std::make_tuple(false, currentpath);
}

VirtualPath DualView::ResolvePathToFolder(DBID id){
    
    ResolvePathInfinityBlocker infinityblocker;

    const auto result = ResolvePathHelperRecursive(id, VirtualPath(""), infinityblocker);

    if(!std::get<0>(result)){

        // Failed //
        return "Recursive Path: " + static_cast<std::string>(std::get<1>(result));
    }

    return std::get<1>(result);
}
// ------------------------------------ //
std::shared_ptr<AppliedTag> DualView::ParseTagWithBreakRule(const std::string &str) const{

    auto rule = _Database->SelectTagBreakRuleByStr(str);

    if(!rule)
        return nullptr;

    std::string tagname;
    std::shared_ptr<Tag> tag;
    auto modifiers = rule->DoBreak(str, tagname, tag);

    if(!tag && modifiers.empty()){

        // Rule didn't match //
        return nullptr;
    }

    return std::make_shared<AppliedTag>(tag, modifiers);
}

std::string DualView::GetExpandedTagFromSuperAlias(const std::string &str) const{

    return _Database->SelectTagSuperAlias(str);
}

std::shared_ptr<AppliedTag> DualView::ParseTagName(const std::string &str) const{

    auto byname = _Database->SelectTagByNameOrAlias(str);

    if(byname)
        return std::make_shared<AppliedTag>(byname);

    // Try to get expanded form //
    const auto expanded = GetExpandedTagFromSuperAlias(str);

    if(!expanded.empty()){

        // Parse the replacement tag //
        return ParseTagFromString(expanded);
    }

    return nullptr;
}

std::tuple<std::shared_ptr<AppliedTag>, std::string, std::shared_ptr<AppliedTag>>
    DualView::ParseTagWithComposite(const std::string &str) const
{
    // First split it into words //
    std::vector<std::string> words;
    Leviathan::StringOperations::CutString<std::string>(str, " ", words);

    // Must be 3 words or more
    if(words.size() < 3)
        return std::make_tuple(nullptr, "", nullptr);

    // Find a middle word for which the following works
    // left side can be parsed with ParseTagWithOnlyModifiers middle word is a word like
    // "on", "with", or anything really and the right side can also be parsed with
    // ParseTagWithOnlyModifiers

    std::vector<std::string*> left;
    std::vector<std::string*> right;

    for(size_t i = 1; i < words.size() - 1; ++i){

        // Build the words //
        size_t insertIndex = 0;
        left.resize(i);

        for(size_t a = 0; a < i; ++a){

            left[insertIndex++] = &words[a];
        }

        std::shared_ptr<AppliedTag> parsedleft;
        std::shared_ptr<AppliedTag> parsedright;
        
        try{
            
            parsedleft = ParseTagFromString(
                Leviathan::StringOperations::StitchTogether<std::string>(left, " "));
            
        } catch(const Leviathan::InvalidArgument){

            // No such tag //
            continue;
        }

        if(!parsedleft)
            continue;

        insertIndex = 0;
        right.resize(words.size() - 1 - i);

        for(size_t a = i + 1; a < words.size(); ++a){

            right[insertIndex++] = &words[a];
        }

        try{
            parsedright = ParseTagFromString(
                Leviathan::StringOperations::StitchTogether<std::string>(right, " "));
            
        } catch(const Leviathan::InvalidArgument){

            // No such tag //
            continue;
        }

        if(!parsedright)
            continue;
        
        auto middle = words[i];

        // It worked //

        // We need to add the right tag to the left one, otherwise things break
        parsedleft->SetCombineWith(middle, parsedright);
        
        return std::make_tuple(parsedleft, middle, parsedright);
    }

    // No matches found //
    return std::make_tuple(nullptr, "", nullptr);
}


std::shared_ptr<AppliedTag> DualView::ParseHelperCheckModifiersAndBreakRules(
    const std::shared_ptr<AppliedTag> &maintag, const std::vector<std::string*> &words,
    bool taglast) const
{
    // All the words need to be modifiers or a break rule must accept them //
    bool success = true;
    std::vector<std::shared_ptr<TagModifier>> modifiers;
    modifiers.reserve(words.size());

    for(size_t a = 0; a < words.size(); ++a){

        auto mod = _Database->SelectTagModifierByNameOrAliasAG(*words[a]);

        if(mod){

            modifiers.push_back(mod);
            continue;
        }

        // This wasn't a modifier //
        auto broken = ParseTagWithBreakRule(*words[a]);

        // Must have parsed a tag that has only modifiers
        if(!broken || broken->GetTag() || broken->GetModifiers().empty()){

            // unknown modifier and no break rule //
            return nullptr;
        }

        // Modifiers from break rule //
        for(const auto& alreadyset : broken->GetModifiers()){
    
            modifiers.push_back(alreadyset);
        }
    }

    for(const auto& alreadyset : maintag->GetModifiers()){
    
        modifiers.push_back(alreadyset);
    }

    return std::make_shared<AppliedTag>(maintag->GetTag(), modifiers);
}

std::shared_ptr<AppliedTag> DualView::ParseTagWithOnlyModifiers(const std::string &str) const{
    
    // First split it into words //
    std::vector<std::string> words;
    Leviathan::StringOperations::CutString<std::string>(str, " ", words);

    // Must be more than 1 word
    if(words.size() < 2)
        return nullptr;

    // Then try to match all the stuff in front of the tag into modifiers and the
    // last word(s) into a tag
    std::vector<std::string*> back;
    std::vector<std::string*> front;
    
    for(size_t i = 0; i < words.size(); ++i){

        // Build the back string with the first excluded word being at words[i]
        size_t insertIndex = 0;
        back.resize(words.size() - 1 - i);

        for(size_t a = i + 1; a < words.size(); ++a){

            back[insertIndex++] = &words[a];
        }

        front.resize(i + 1);
        insertIndex = 0;
        
        for(size_t a = 0; a <= i; ++a){

            front[insertIndex++] = &words[a];
        }
        
        
        // Create strings //
        auto backStr = Leviathan::StringOperations::StitchTogether<std::string>(back, " ");

        // Back needs to be a tag //
        auto tag = ParseTagName(backStr);

        if(!tag){

            // Maybe tag is first //
            auto frontStr = Leviathan::StringOperations::StitchTogether<std::string>(
                front, " ");

            tag = ParseTagName(frontStr);

            if(!tag)
                continue;

            // Try parsing it //
            auto finished = ParseHelperCheckModifiersAndBreakRules(tag, back, false);

            if(finished)
                return finished;
            
            continue;
        }

        auto finished = ParseHelperCheckModifiersAndBreakRules(tag, front, false);

        if(finished){

            // It succeeded //
            return finished;
        }
    }

    return nullptr;
}

std::shared_ptr<AppliedTag> DualView::ParseTagFromString(std::string str) const{

    // Strip whitespace //
    Leviathan::StringOperations::RemovePreceedingTrailingSpaces(str);

    if(str.empty())
        return nullptr;

    // Convert to lower //
    str = StringToLower(str);

    // Exact match a tag //
    auto existingtag = ParseTagName(str);

    if(existingtag)
        return existingtag;

    // Wasn't exactly a single tag //
    
    // Check does removing whitespace create some existing tag
    auto nowhitespace = Leviathan::StringOperations::RemoveCharacters<std::string>(str, " ");

    if(nowhitespace.size() != str.size()){
        
        existingtag = ParseTagName(nowhitespace);

        if(existingtag)
            return existingtag;
    }

    // Modifier before tag //
    auto modifiedtag = ParseTagWithOnlyModifiers(str);

    if(modifiedtag)
        return modifiedtag;

    // Detect a composite //
    std::vector<std::string> words;
    Leviathan::StringOperations::CutString<std::string>(str, " ", words);

    if(!words.empty()){
        
        // Try to break composite //
        auto composite = ParseTagWithComposite(str);

        if(std::get<0>(composite)){
            
            return std::get<0>(composite);
        }
    }

    // Break rules are detected by ParseTagName
    
    // Check does removing ending 's' create an existing tag
    if(str.back() == 's'){
        
        return ParseTagFromString(str.substr(0, str.size() - 1));
    }

    throw Leviathan::InvalidArgument("unknown tag '" + str + "'");
}
// ------------------------------------ //
//#define GETSUGGESTIONS_DEBUG

#ifdef GETSUGGESTIONS_DEBUG

#define SUGG_DEBUG(x) LOG_WRITE("Suggestions" + std::string(x));

#else
#define SUGG_DEBUG(x) {}

#endif

std::vector<std::string> DualView::GetSuggestionsForTag(std::string str,
    size_t maxcount /*= 100*/) const
{
    // Strip whitespace //
    Leviathan::StringOperations::RemovePreceedingTrailingSpaces(str);

    if(str.empty())
        return {};

    // Convert to lower //
    str = StringToLower(str);

    SUGG_DEBUG("Gettings suggestions for: " + str);

    std::vector<std::string> result;
    
    Leviathan::StringIterator itr(str);

    std::string currentpart;
    std::string prefix;

    bool modifierallowed = true;
    bool tagallowed = true;

    // Consume all the valid parts from the front //
    while(!itr.IsOutOfBounds()){

        auto currentword = itr.GetUntilNextCharacterOrAll<std::string>(' ');

        if(!currentword)
            continue;

        currentpart += currentpart.empty() ? *currentword : " " + *currentword;

        if(false){

        thingwasvalidlabel:
            SUGG_DEBUG("Part was valid: " + currentpart);
            prefix += prefix.empty() ? currentpart : " " + currentpart;
            currentpart.clear();
            continue;
        }

        if(tagallowed){
            auto byname = _Database->SelectTagByNameOrAlias(currentpart);
            if(byname){

                tagallowed = false;
                modifierallowed = false;
                goto thingwasvalidlabel;
            }
        }

        if(modifierallowed){
            
            auto mod = _Database->SelectTagModifierByNameOrAliasAG(currentpart);
            if(mod){

                modifierallowed = false;
                tagallowed = true;
                goto thingwasvalidlabel;
            }
        }

        auto rule = _Database->SelectTagBreakRuleByStr(currentpart);

        if(rule){

            std::string tagname;
            std::shared_ptr<Tag> tag;
            auto modifiers = rule->DoBreak(currentpart, tagname, tag);

            if(tag || !modifiers.empty()){

                // Rule matched
                modifierallowed = false;
                tagallowed = true;
                goto thingwasvalidlabel;
            }
        }

        auto alias = _Database->SelectTagSuperAlias(currentpart);
        if(!alias.empty()){
            modifierallowed = false;
            tagallowed = true;
            goto thingwasvalidlabel;
        }

        // TODO: check combined with
    }

    // Add space after prefix //
    if(!prefix.empty())
        prefix += " ";
    
    SUGG_DEBUG("Finished parsing and valid prefix is: " + prefix);
    SUGG_DEBUG("Unparsed part is: " + currentpart);
    
    // If there's nothing left in currentpart the tag would be successfully parsed
    // So just make sure that it is and at it to the result
    if(currentpart.empty()){

        try{
            auto parsed = ParseTagFromString(str);

            if(parsed)
                result.push_back(str);
            
        } catch(...){

            // Not actually a tag
            LOG_WARNING("Get suggestions thought \"" + str + "\" would be a valid tag "
                "but it isn't");
        }

        // Also we want to get longer tags that start with the same thing //
        RetrieveTagsMatching(result, str);
        
    } else {

        // Get suggestions for it //
        {
            SUGG_DEBUG("Finding suggestions: " + currentpart);
            
            std::vector<std::string> tmpholder;
            RetrieveTagsMatching(tmpholder, currentpart);

            result.reserve(result.size() + tmpholder.size());

            for(const auto& gotmatch : tmpholder){

                SUGG_DEBUG("Found suggestion: " + gotmatch + ", prefix: " + prefix);
                result.push_back(prefix + gotmatch);
            }
        }

        // Combines //
        const auto tail = Leviathan::StringOperations::RemoveFirstWords(currentpart, 1);

        if(tail != currentpart){

            SUGG_DEBUG("Finding combine suggestions: " + tail);

            const auto tailprefix = prefix + currentpart.substr(0, currentpart.size()
                - tail.size());

            std::vector<std::string> tmpholder = GetSuggestionsForTag(tail,
                std::max(maxcount - result.size(), maxcount / 4));

            result.reserve(result.size() + tmpholder.size());

            for(const auto& gotmatch : tmpholder){

                SUGG_DEBUG("Found combine suggestion: " + gotmatch + ", prefix: " + tailprefix);
                result.push_back(tailprefix + gotmatch);
            } 
        } else {

            
        }
    }

    // Sort the most relevant results first //
    DV::SortSuggestions(result.begin(), result.end(), str);
    
    result.erase(std::unique(result.begin(), result.end()), result.end());
    
    if(result.size() > maxcount)
        result.resize(maxcount);

    SUGG_DEBUG("Resulting suggestions: " + Convert::ToString(result.size()));
    for(auto iter = result.begin(); iter != result.end(); ++iter)
        SUGG_DEBUG(" " + *iter);
    
    return result;
}
// ------------------------------------ //
void DualView::RetrieveTagsMatching(std::vector<std::string> &result,
    const std::string &str) const
{
    _Database->SelectTagNamesWildcard(result, str);
    
    _Database->SelectTagAliasesWildcard(result, str);

    _Database->SelectTagModifierNamesWildcard(result, str);
    
    _Database->SelectTagBreakRulesByStrWildcard(result, str);

    _Database->SelectTagSuperAliasWildcard(result, str);
}


// ------------------------------------ //
// Gtk callbacks
void DualView::OpenImageFile_OnClick(){

    Gtk::FileChooserDialog dialog("Choose an image to open",
        Gtk::FILE_CHOOSER_ACTION_OPEN);
    dialog.set_transient_for(*MainMenu);

    //Add response buttons the the dialog:
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Open", Gtk::RESPONSE_OK);

    //Add filters, so that only certain file types can be selected:
    auto filter_image = Gtk::FileFilter::create();
    filter_image->set_name("Image Files");

    for(const auto& type : SUPPORTED_EXTENSIONS){

        filter_image->add_mime_type(std::get<1>(type));
    }
    dialog.add_filter(filter_image);

    auto filter_any = Gtk::FileFilter::create();
    filter_any->set_name("Any files");
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);

    // Wait for a selection
    int result = dialog.run();

    //Handle the response:
    switch(result)
    {
    case(Gtk::RESPONSE_OK):
    {
        std::string filename = dialog.get_filename();

        if(filename.empty())
            return;
        
        OpenImageViewer(filename);
        return;
    }
    case(Gtk::RESPONSE_CANCEL):
    default:
    {
        // Canceled / nothing selected //
        return;
    }
    }
}

void DualView::OpenCollection_OnClick(){

    // Show it //
    Application->add_window(*_CollectionView);
    _CollectionView->show();
    _CollectionView->present();
}

void DualView::OpenDebug_OnClick(){

    Application->add_window(*_DebugWindow);
    _DebugWindow->show();
    _DebugWindow->present();
}
// ------------------------------------ //
std::string DualView::MakePathUniqueAndShort(const std::string &path){

    const auto original = boost::filesystem::path(path);
    
    // First cut it //
    const auto length = boost::filesystem::absolute(original).string().size();

    const auto extension = original.extension();
    const auto baseFolder = original.parent_path();
    const auto fileName = original.stem();
    
    if(length > DUALVIEW_MAX_ALLOWED_PATH){

        std::string name = fileName.string();
        name = name.substr(0, name.size() / 2);
        return MakePathUniqueAndShort((baseFolder / (name + extension.string())).string());
    }

    // Then make sure it doesn't exist //
    if(!boost::filesystem::exists(original)){

        return original.c_str();
    }
    
    long number = 0;

    boost::filesystem::path finaltarget;

    do{
        
        finaltarget = baseFolder / (fileName.string() + "_" + Convert::ToString(++number) +
            extension.string());

    } while(boost::filesystem::exists(finaltarget));

    // Make sure it is still short enough
    return MakePathUniqueAndShort(finaltarget.string());
}

std::string DualView::CalculateBase64EncodedHash(const std::string &str){

    // Calculate sha256 hash //
    byte digest[CryptoPP::SHA256::DIGESTSIZE];

    CryptoPP::SHA256().CalculateDigest(digest, reinterpret_cast<const byte*>(
            str.data()), str.length());

    static_assert(sizeof(digest) == CryptoPP::SHA256::DIGESTSIZE, "sizeof funkyness");

    // Encode it //
    std::string hash = base64_encode(digest, sizeof(digest));

    // Make it path safe //
    return Leviathan::StringOperations::ReplaceSingleCharacter<std::string>(hash, "/", '_');
}

