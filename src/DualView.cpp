// ------------------------------------ //
#include "DualView.h"

#include <chrono>
#include <iostream>

#include <boost/filesystem.hpp>
#include <cryptopp/sha.h>

#include "Common.h"

#include "Common/StringOperations.h"
#include "Iterators/StringIterator.h"
#include "resources/Collection.h"
#include "resources/Folder.h"
#include "resources/Image.h"
#include "resources/Tags.h"
#include "windows/ActionEditors.h"
#include "windows/AddToFolder.h"
#include "windows/AlreadyImportedImageDeleter.h"
#include "windows/CollectionView.h"
#include "windows/DebugWindow.h"
#include "windows/Downloader.h"
#include "windows/DownloadItemEditor.h"
#include "windows/DownloadSetup.h"
#include "windows/DuplicateFinder.h"
#include "windows/FolderCreator.h"
#include "windows/ImageFinder.h"
#include "windows/Importer.h"
#include "windows/MaintenanceTools.h"
#include "windows/RemoveFromFolders.h"
#include "windows/RenameWindow.h"
#include "windows/Reorder.h"
#include "windows/SingleCollection.h"
#include "windows/SingleView.h"
#include "windows/TagManager.h"
#include "windows/Undo.h"

#include "base64.h"
#include "CacheManager.h"
#include "ChangeEvents.h"
#include "CurlWrapper.h"
#include "Database.h"
#include "DownloadManager.h"
#include "Exceptions.h"
#include "PluginManager.h"
#include "Settings.h"
#include "UtilityHelpers.h"

using namespace DV;
// ------------------------------------ //

constexpr auto MAX_INVOKES_PER_CALL = 200;

//! Used for thread detection
thread_local static int32_t ThreadSpecifier = 0;

//! Asserts if not called on the main thread
inline void AssertIfNotMainThread()
{
    LEVIATHAN_ASSERT(DualView::IsOnMainThread(), "Function called on the wrong thread");
}

DualView::DualView(Glib::RefPtr<Gtk::Application> app) : Application(app)
{
    Staticinstance = this;
    ThreadSpecifier = MAIN_THREAD_MAGIC;

    // Listen for open events //
    app->signal_activate().connect(sigc::mem_fun(*this, &DualView::_OnInstanceLoaded));
    app->signal_command_line().connect(sigc::mem_fun(*this, &DualView::_HandleCmdLine), false);

    // This is called when the application is ran with a file //
    app->signal_open().connect(sigc::mem_fun(*this, &DualView::_OnSignalOpen));

    app->signal_handle_local_options().connect(sigc::mem_fun(*this, &DualView::_OnPreParseCommandLine));
}

DualView::~DualView()
{
    QuitWorkerThreads = true;

    if (_CacheManager)
    {
        // Cache manager start close //
        _CacheManager->QuitProcessingThreads();
    }

    // Signal downloads to end after the next one
    if (_DownloadManager)
        _DownloadManager->StopDownloads();

    if (!IsInitialized)
    {
        _WaitForWorkerThreads();
        _CacheManager.reset();
        _Settings.reset();

        // Reset staticinstance only if it is still us. This is for running tests
        if (Staticinstance == this)
            Staticinstance = nullptr;

        if (!SuppressSecondInstance)
            std::cout << "DualView++ Main Instance Notified (If there were no errors). \n"
                         "Extra instance quitting"
                      << std::endl;

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

    // Worker threads might still be working on some stuff
    _WaitForWorkerThreads();

    // Unload image loader. All images must be closed before this is called //
    _CacheManager.reset();

    _Settings.reset();

    // Let go of last database resources //
    UncategorizedCollection.reset();
    RootFolder.reset();

    // Close database //
    _Database.reset();

    _ChangeEvents.reset();

    Staticinstance = nullptr;
}

DualView::DualView(bool tests, const std::string& dbfile)
{
    _Logger = std::make_unique<Leviathan::Logger>("test_log.txt");

    LEVIATHAN_ASSERT(tests, "DualView test constructor called with false");

    SuppressSecondInstance = true;

    Staticinstance = this;
    ThreadSpecifier = MAIN_THREAD_MAGIC;

    _CacheManager = std::make_unique<CacheManager>();

    _Settings = std::make_unique<Settings>("test_settings.levof");

    _ChangeEvents = std::make_unique<ChangeEvents>();

    if (!dbfile.empty())
        _Database = std::make_unique<Database>(dbfile);

    _StartWorkerThreads();
}

DualView::DualView(bool tests, bool memorysettings)
{
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

DualView::DualView(std::string tests, std::unique_ptr<Database>&& db /*= nullptr*/)
{
    _Logger = std::make_unique<Leviathan::Logger>("empty_dualview_log.txt");

    if (db)
        _Database = std::move(db);

    _ChangeEvents = std::make_unique<ChangeEvents>();

    _Settings = std::make_unique<Settings>("memory");

    LEVIATHAN_ASSERT(tests == "empty", "DualView test constructor called with not empty");

    SuppressSecondInstance = true;

    Staticinstance = this;
    ThreadSpecifier = MAIN_THREAD_MAGIC;
}

DualView* DualView::Staticinstance = nullptr;

DualView& DualView::Get()
{
    LEVIATHAN_ASSERT(Staticinstance, "DualView static instance is null");
    return *Staticinstance;
}

bool DualView::IsOnMainThread()
{
    return ThreadSpecifier == MAIN_THREAD_MAGIC;
}

void DualView::IsOnMainThreadAssert()
{
    LEVIATHAN_ASSERT(DualView::IsOnMainThread(), "Function not called on the main thread");
}

// ------------------------------------ //
void DualView::_OnInstanceLoaded()
{
    if (IsInitialized)
    {
        return;
    }

    AssertIfNotMainThread();

    _Logger = std::make_unique<Leviathan::Logger>("log.txt");

    if (!_Logger)
    {
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

    // Load custom widget styles
    auto screen = Gdk::Screen::get_default();

    CustomCSS = Gtk::CssProvider::create();
    CustomCSS->load_from_resource("/com/boostslair/dualviewpp/resources/dualview.css");

    StyleContext = Gtk::StyleContext::create();

    StyleContext->add_provider_for_screen(screen, CustomCSS, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    MainBuilder = Gtk::Builder::create_from_resource("/com/boostslair/dualviewpp/resources/gui/main_gui.glade");

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

    OpenImageFile->signal_clicked().connect(sigc::mem_fun(*this, &DualView::OpenImageFile_OnClick));

    Gtk::Button* OpenCollection = nullptr;
    MainBuilder->get_widget("OpenCollection", OpenCollection);
    LEVIATHAN_ASSERT(OpenCollection, "Invalid .glade file");

    OpenCollection->signal_clicked().connect(sigc::mem_fun(*this, &DualView::OpenCollection_OnClick));

    Gtk::ToolButton* OpenDebug = nullptr;
    MainBuilder->get_widget("OpenDebug", OpenDebug);
    LEVIATHAN_ASSERT(OpenDebug, "Invalid .glade file");

    OpenDebug->signal_clicked().connect(sigc::mem_fun(*this, &DualView::OpenDebug_OnClick));

    Gtk::ToolButton* OpenUndo = nullptr;
    MainBuilder->get_widget("OpenUndoWindow", OpenUndo);
    LEVIATHAN_ASSERT(OpenUndo, "Invalid .glade file");

    OpenUndo->signal_clicked().connect(sigc::mem_fun(*this, &DualView::OpenUndoWindow_OnClick));

    Gtk::ToolButton* openMaintenance = nullptr;
    MainBuilder->get_widget("OpenMaintenance", openMaintenance);
    LEVIATHAN_ASSERT(openMaintenance, "Invalid .glade file");

    openMaintenance->signal_clicked().connect(sigc::mem_fun<void>(*this, &DualView::OpenMaintenance_OnClick));

    Gtk::Button* OpenImporter = nullptr;
    MainBuilder->get_widget("OpenImporter", OpenImporter);
    LEVIATHAN_ASSERT(OpenImporter, "Invalid .glade file");

    OpenImporter->signal_clicked().connect(sigc::mem_fun<void>(*this, &DualView::OpenImporter));

    Gtk::Button* OpenTags = nullptr;
    MainBuilder->get_widget("OpenTags", OpenTags);
    LEVIATHAN_ASSERT(OpenTags, "Invalid .glade file");

    OpenTags->signal_clicked().connect(sigc::mem_fun<void>(*this, &DualView::OpenTagCreator));

    Gtk::Button* OpenDownloader = nullptr;
    MainBuilder->get_widget("OpenDownloader", OpenDownloader);
    LEVIATHAN_ASSERT(OpenDownloader, "Invalid .glade file");

    OpenDownloader->signal_clicked().connect(sigc::mem_fun<void>(*this, &DualView::OpenDownloader));

    Gtk::Button* OpenImageFinder = nullptr;
    MainBuilder->get_widget("OpenImageFinder", OpenImageFinder);
    LEVIATHAN_ASSERT(OpenImageFinder, "Invalid .glade file");

    OpenImageFinder->signal_clicked().connect(sigc::mem_fun<void>(*this, &DualView::OpenImageFinder));

    Gtk::Button* openDuplicateFinder = nullptr;
    MainBuilder->get_widget("OpenDuplicateFinder", openDuplicateFinder);
    LEVIATHAN_ASSERT(openDuplicateFinder, "Invalid .glade file");

    openDuplicateFinder->signal_clicked().connect(sigc::mem_fun<void>(*this, &DualView::OpenDuplicateFinder_OnClick));

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

    // Debug window
    DebugWindow* tmpDebug = nullptr;
    MainBuilder->get_widget_derived("DebugWindow", tmpDebug);
    LEVIATHAN_ASSERT(tmpDebug, "Invalid .glade file");

    // Store the window //
    _DebugWindow = std::shared_ptr<DebugWindow>(tmpDebug);

    // Already imported
    AlreadyImportedImageDeleter* tmpAlreadyImported = nullptr;
    MainBuilder->get_widget_derived("AlreadyImportedImageDeleter", tmpAlreadyImported);
    LEVIATHAN_ASSERT(tmpAlreadyImported, "Invalid .glade file");

    // Store the window //
    _AlreadyImportedImageDeleter = std::shared_ptr<AlreadyImportedImageDeleter>(tmpAlreadyImported);

    // Maintenance window
    MaintenanceTools* tmpMaintenance = nullptr;
    MainBuilder->get_widget_derived("MaintenanceTools", tmpMaintenance);
    LEVIATHAN_ASSERT(tmpMaintenance, "Invalid .glade file");

    // Store the window //
    _MaintenanceTools = std::shared_ptr<MaintenanceTools>(tmpMaintenance);

    // Initialize accelerator keys here
    // Gtk::AccelMap::add_entry("<CollectionList-Item>/Right/Help", GDK_KEY_H,
    //    Gdk::CONTROL_MASK);

    // Start worker threads //
    _StartWorkerThreads();

    LOG_INFO("Basic initialization completed");

    IsInitialized = true;
}

bool DualView::_DoInitThreadAction()
{
    // Load settings //
    try
    {
        _Settings = std::make_unique<Settings>("dv_settings.levof");
    }
    catch (const Leviathan::InvalidArgument& e)
    {
        LOG_ERROR("Invalid configuration. Please delete it and try again:");
        e.PrintToLog();
        return true;
    }

    _Settings->VerifyFoldersExist();

    // Load curl //
    _CurlWrapper = std::make_unique<CurlWrapper>();

    // Load plugins //
    const auto plugins = _Settings->GetPluginList();

    if (!plugins.empty())
    {
        const auto pluginFolder = boost::filesystem::path(_Settings->GetPluginFolder());

        LOG_INFO("Loading " + Convert::ToString(plugins.size()) + " plugin(s)");

        for (const auto& plugin : plugins)
        {
            // Plugin name
#ifdef _WIN32
            const auto libname = plugin + ".dll";
#else
            const auto libname = "lib" + plugin + ".so";
#endif //_WIN32

            if (!_PluginManager->LoadPlugin((pluginFolder / libname).string()))
            {
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

    _Database->SetMaxActionHistory(_Settings->GetActionHistorySize());

    try
    {
        _Database->Init();
    }
    catch (const Leviathan::InvalidState& e)
    {
        LOG_ERROR("Database initialization failed: ");
        e.PrintToLog();
        return true;
    }
    catch (const InvalidSQL& e)
    {
        LOG_ERROR("Database initialization logic has a bug, sql error: ");
        e.PrintToLog();
        return true;
    }

    DatabaseThread = std::thread(&DualView::_RunDatabaseThread, this);

    // Succeeded //
    return false;
}

void DualView::_RunInitThread()
{
    LOG_INFO("Running Init thread");
    LoadError = false;

    DateInitThread = std::thread(
        []() -> void
        {
            // Load timezone database
            TimeHelpers::TimeZoneDatabaseSetup();
        });

    bool result = _DoInitThreadAction();

    if (result)
    {
        // Sleep if an error occured to let the user read it
        LoadError = true;

        // std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    }

    // Invoke the callback on the main thread //
    StartDispatcher.emit();
}

void DualView::_OnLoadingFinished()
{
    AssertIfNotMainThread();

    // The thread needs to be joined or an exception is thrown
    if (LoadThread.joinable())
        LoadThread.join();

    if (LoadError)
    {
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
    // OpenImageViewer("/home/hhyyrylainen/690806.jpg");
    // OpenImporter();
}

// ------------------------------------ //
int DualView::_HandleCmdLine(const Glib::RefPtr<Gio::ApplicationCommandLine>& command_line)
{
    // First handle already parsed things //
    auto alreadyParsed = command_line->get_options_dict();

    Glib::ustring referrer;
    if (!alreadyParsed->lookup_value("dl-referrer", referrer))
    {
        referrer = "";
    }

    Glib::ustring fileUrl;
    if (alreadyParsed->lookup_value("dl-image", fileUrl))
    {
        InvokeFunction(
            [=]() -> void
            {
                LOG_INFO("File to download: " + std::string(fileUrl.c_str()));
                OnNewImageLinkReceived(fileUrl, referrer);
            });
    }

    if (alreadyParsed->lookup_value("dl-page", fileUrl))
    {
        InvokeFunction(
            [=]() -> void
            {
                LOG_INFO("Page to download: " + std::string(fileUrl.c_str()));
                OnNewGalleryLinkReceived(fileUrl);
            });
    }

    if (alreadyParsed->lookup_value("dl-auto", fileUrl))
    {
        InvokeFunction(
            [=]() -> void
            {
                LOG_INFO("Auto detect and download: " + std::string(fileUrl.c_str()));

                // If the file has a content extension then open as an image
                const auto extension =
                    Leviathan::StringOperations::GetExtension(DownloadManager::ExtractFileName(fileUrl.c_str()));

                if (IsExtensionContent("." + extension))
                {
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

    if (argc > 1)
    {
        std::cout << "Extra arguments: " << std::endl;

        for (int i = 1; i < argc; ++i)
            std::cout << argv[i] << (i + 1 < argc ? ", " : " ");

        std::cout << std::endl;

        std::cout << "TODO: check if it is a file and open" << std::endl;
    }

    g_strfreev(argv);

    Application->activate();

    return 0;
}

int DualView::_OnPreParseCommandLine(const Glib::RefPtr<Glib::VariantDict>& options)
{
    bool version;
    if (options->lookup_value("version", version))
    {
        std::cout << "DualView++ Version " << DUALVIEW_VERSION << std::endl;
        SuppressSecondInstance = true;
        return 0;
    }

    return -1;
}

// ------------------------------------ //
void DualView::_OnSignalOpen(const std::vector<Glib::RefPtr<Gio::File>>& files, const Glib::ustring& stuff)
{
    LOG_INFO("Got file list to open:");

    for (const auto& file : files)
    {
        LOG_WRITE("\t" + file->get_path());
    }
}

// ------------------------------------ //
void DualView::_HandleMessages()
{
    AssertIfNotMainThread();

    std::lock_guard<std::mutex> lock(MessageQueueMutex);

    // Handle all messages, because we might not get a dispatch for each message
    while (!CloseEvents.empty())
    {
        auto event = CloseEvents.front();
        CloseEvents.pop_front();

        // Handle the event
        // Find the window //
        for (auto iter = OpenWindows.begin(); iter != OpenWindows.end(); ++iter)
        {
            if ((*iter).get() == event->AffectedWindow)
            {
                LOG_INFO("DualView: notified of a closed window");
                OpenWindows.erase(iter);
                break;
            }
        }
    }
}

// ------------------------------------ //
void DualView::QueueImageHashCalculate(std::shared_ptr<Image> img)
{
    std::lock_guard<std::mutex> lock(HashImageQueueMutex);

    HashImageQueue.push_back(img);

    HashCalculationThreadNotify.notify_all();
}

void DualView::_RunHashCalculateThread()
{
    std::unique_lock<std::mutex> lock(HashImageQueueMutex);

    while (!QuitWorkerThreads)
    {
        if (HashImageQueue.empty())
            HashCalculationThreadNotify.wait(lock);

        while (!HashImageQueue.empty())
        {
            auto img = HashImageQueue.front().lock();

            HashImageQueue.pop_front();

            if (!img)
            {
                // Image has been deallocated already //
                continue;
            }

            lock.unlock();

            img->_DoHashCalculation();

            lock.lock();

            // Replace with an existing image if the hash exists //
            try
            {
                auto existing = _Database->SelectImageByHashAG(img->GetHash());

                if (existing)
                {
                    LOG_INFO("Calculated hash for a duplicate image");
                    img->BecomeDuplicateOf(*existing);
                    continue;
                }
            }
            catch (const InvalidSQL&)
            {
                // Database probably isn't initialized
            }
            catch (const Leviathan::InvalidState& e)
            {
                LOG_ERROR("Image hash calculation failed, exception:");
                e.PrintToLog();
            }

            img->_OnFinishHash();
        }
    }
}

// ------------------------------------ //
void DualView::QueueDBThreadFunction(std::function<void()> func, int64_t priority /*= -1*/)
{
    if (priority == -1)
        priority = TimeHelpers::GetCurrentUnixTimestamp();

    GUARD_LOCK_OTHER(DatabaseFuncQueue);

    DatabaseFuncQueue.Push(guard, std::make_unique<std::function<void()>>(func), priority);

    DatabaseThreadNotify.notify_all();
}

void DualView::QueueWorkerFunction(std::function<void()> func, int64_t priority /*= -1*/)
{
    if (priority == -1)
        priority = TimeHelpers::GetCurrentUnixTimestamp();

    GUARD_LOCK_OTHER(WorkerFuncQueue);

    WorkerFuncQueue.Push(guard, std::make_unique<std::function<void()>>(func), priority);

    WorkerThreadNotify.notify_one();
}

void DualView::QueueConditional(std::function<bool()> func)
{
    std::lock_guard<std::mutex> lock(ConditionalFuncQueueMutex);

    ConditionalFuncQueue.push_back(std::make_shared<std::function<bool()>>(func));

    ConditionalWorkerThreadNotify.notify_one();
}

void DualView::_RunDatabaseThread()
{
    GUARD_LOCK_OTHER(DatabaseFuncQueue);

    while (!QuitWorkerThreads)
    {
        if (DatabaseFuncQueue.Empty(guard))
            DatabaseThreadNotify.wait(guard);

        while (auto task = DatabaseFuncQueue.Pop(guard))
        {
            guard.unlock();

            task->Task->operator()();
            task->OnDone();

            guard.lock();
        }
    }
}

void DualView::_RunWorkerThread()
{
    GUARD_LOCK_OTHER(WorkerFuncQueue);

    while (!QuitWorkerThreads)
    {
        if (WorkerFuncQueue.Empty(guard))
            WorkerThreadNotify.wait(guard);

        while (auto task = WorkerFuncQueue.Pop(guard))
        {
            guard.unlock();

            task->Task->operator()();
            task->OnDone();

            guard.lock();
        }
    }
}

void DualView::_RunConditionalThread()
{
    std::unique_lock<std::mutex> lock(ConditionalFuncQueueMutex);

    while (!QuitWorkerThreads)
    {
        if (ConditionalFuncQueue.empty())
            ConditionalWorkerThreadNotify.wait(lock);

        for (size_t i = 0; i < ConditionalFuncQueue.size();)
        {
            const auto func = ConditionalFuncQueue[i];

            lock.unlock();

            const auto result = func->operator()();

            lock.lock();

            if (result)
            {
                // Remove //
                ConditionalFuncQueue.erase(ConditionalFuncQueue.begin() + i);
            }
            else
            {
                ++i;
            }
        }

        // Don't constantly check whether functions are ready to run
        if (!ConditionalFuncQueue.empty() && !QuitWorkerThreads)
            ConditionalWorkerThreadNotify.wait_for(lock, std::chrono::milliseconds(12));
    }
}

// ------------------------------------ //
void DualView::InvokeFunction(std::function<void()> func)
{
    std::unique_lock<std::mutex> lock(InvokeQueueMutex);

    InvokeQueue.push_back(func);

    // Notify main thread
    InvokeDispatcher.emit();
}

void DualView::RunOnMainThread(const std::function<void()>& func)
{
    if (IsOnMainThread())
    {
        func();
    }
    else
    {
        InvokeFunction(func);
    }
}

void DualView::_ProcessInvokeQueue()
{
    // Wait until completely loaded before invoking
    if (!LoadCompletelyFinished)
        return;

    std::unique_lock<std::mutex> lock(InvokeQueueMutex);

    // To not completely lock up the main thread, there's a max  number of invokes to process
    // at once
    int processedInvokes = 0;

    while (!InvokeQueue.empty())
    {
        const auto func = InvokeQueue.front();

        InvokeQueue.pop_front();

        lock.unlock();

        func();

        lock.lock();

        if (++processedInvokes >= MAX_INVOKES_PER_CALL)
            break;
    }
}

// ------------------------------------ //
void DualView::_StartWorkerThreads()
{
    QuitWorkerThreads = false;

    HashCalculationThread = std::thread(std::bind(&DualView::_RunHashCalculateThread, this));
    Worker1Thread = std::thread(std::bind(&DualView::_RunWorkerThread, this));
    ConditionalWorker1 = std::thread(std::bind(&DualView::_RunConditionalThread, this));
}

void DualView::_WaitForWorkerThreads()
{
    // Make sure this is set //
    QuitWorkerThreads = true;

    HashCalculationThreadNotify.notify_all();
    DatabaseThreadNotify.notify_all();
    WorkerThreadNotify.notify_all();
    ConditionalWorkerThreadNotify.notify_all();

    if (HashCalculationThread.joinable())
        HashCalculationThread.join();

    if (DatabaseThread.joinable())
        DatabaseThread.join();

    if (DateInitThread.joinable())
        DateInitThread.join();

    if (Worker1Thread.joinable())
        Worker1Thread.join();

    if (ConditionalWorker1.joinable())
        ConditionalWorker1.join();
}

// ------------------------------------ //
std::string DualView::GetPathToCollection(bool isprivate) const
{
    if (isprivate)
    {
        return _Settings->GetPrivateCollection();
    }
    else
    {
        return _Settings->GetPublicCollection();
    }
}

bool DualView::MoveFileToCollectionFolder(std::shared_ptr<Image> img, std::shared_ptr<Collection> collection, bool move)
{
    std::string targetfolder = "";

    // Special case, uncategorized //
    if (collection->GetID() == DATABASE_UNCATEGORIZED_COLLECTION_ID ||
        collection->GetID() == DATABASE_UNCATEGORIZED_PRIVATECOLLECTION_ID)
    {
        targetfolder =
            (boost::filesystem::path(GetPathToCollection(collection->GetIsPrivate())) / "no_category/").c_str();
    }
    else
    {
        targetfolder = (boost::filesystem::path(GetPathToCollection(collection->GetIsPrivate())) / "collections" /
            collection->GetNameForFolder())
                           .c_str();
    }

    if(boost::filesystem::exists(targetfolder))
    {
        // Skip if already there //
        if (boost::filesystem::equivalent(
                targetfolder, boost::filesystem::path(img->GetResourcePath()).remove_filename()))
        {
            return true;
        }
    }

    const auto targetPath =
        boost::filesystem::path(targetfolder) / boost::filesystem::path(img->GetResourcePath()).filename();

    // Make short enough and unique //
    const auto finalPath = MakePathUniqueAndShort(targetPath.string(), true);

    // Target folder may be changed by MakePathUniqueAndShort, so we only create the folder after that
    targetfolder = boost::filesystem::path(finalPath).parent_path().string();

    boost::filesystem::create_directories(targetfolder);

    try
    {
        if (move)
        {
            if (!MoveFile(img->GetResourcePath(), finalPath))
            {
                LOG_ERROR("Failed to move file to collection: " + img->GetResourcePath() + " -> " + finalPath);
                return false;
            }
        }
        else
        {
            boost::filesystem::copy_file(img->GetResourcePath(), finalPath);
        }
    }
    catch (const boost::filesystem::filesystem_error& e)
    {
        LOG_ERROR("Failed to copy file to collection: " + img->GetResourcePath() + " -> " + finalPath);
        LOG_WRITE("Exception: " + std::string(e.what()));
        return false;
    }

    LEVIATHAN_ASSERT(boost::filesystem::exists(finalPath), "Move to collection, final path doesn't exist after copy");

    // Notify image cache that the file was moved //
    if (move)
    {
        LOG_INFO("Moved file to collection. From: " + img->GetResourcePath() + ", Target: " + finalPath);

        _CacheManager->NotifyMovedFile(img->GetResourcePath(), finalPath);
    }

    img->SetResourcePath(finalPath);
    return true;
}

bool DualView::MoveFile(const std::string& original, const std::string& targetname)
{
    // First try renaming //
    try
    {
        boost::filesystem::rename(original, targetname);

        // Renaming succeeded //
        return true;
    }
    catch (const boost::filesystem::filesystem_error&)
    {
    }

    // Rename failed, we need to copy the file and delete the original //
    try
    {
        boost::filesystem::copy_file(original, targetname);

        // Make sure copy worked before deleting original //
        if (boost::filesystem::file_size(original) != boost::filesystem::file_size(targetname))
        {
            LOG_ERROR("File copy: new file is of different size");
            return false;
        }

        boost::filesystem::remove(original);
    }
    catch (const boost::filesystem::filesystem_error&)
    {
        // Failed //
        throw;
    }

    return true;
}

// ------------------------------------ //
bool DualView::IsExtensionContent(const std::string& extension)
{
    for (const auto& type : SUPPORTED_EXTENSIONS)
    {
        if (std::get<0>(type) == extension)
            return true;
    }

    return false;
}

bool DualView::IsFileContent(const std::string& file)
{
    std::string extension = StringToLower(boost::filesystem::path(file).extension().string());

    return IsExtensionContent(extension);
}

// ------------------------------------ //
bool DualView::OpenImageViewer(const std::string& file)
{
    AssertIfNotMainThread();

    LOG_INFO("Opening single image for viewing: " + file);

    OpenImageViewer(Image::Create(file), nullptr);
    return true;
}

void DualView::OpenImageViewer(std::shared_ptr<Image> image, std::shared_ptr<ImageListScroll> scroll)
{
    AssertIfNotMainThread();

    auto builder = Gtk::Builder::create_from_resource("/com/boostslair/dualviewpp/resources/gui/single_view.glade");

    SingleView* window;
    builder->get_widget_derived("SingleView", window);

    if (!window)
    {
        LOG_ERROR("SingleView window GUI layout is invalid");
        return;
    }

    std::shared_ptr<SingleView> wrapped(window);
    _AddOpenWindow(wrapped, *window);
    wrapped->show();

    wrapped->Open(image, scroll);
}

void DualView::OpenSingleCollectionView(std::shared_ptr<Collection> collection)
{
    AssertIfNotMainThread();

    auto builder =
        Gtk::Builder::create_from_resource("/com/boostslair/dualviewpp/resources/gui/single_collection.glade");

    SingleCollection* window;
    builder->get_widget_derived("SingleCollection", window);

    if (!window)
    {
        LOG_ERROR("SingleCollection window GUI layout is invalid");
        return;
    }

    std::shared_ptr<SingleCollection> wrapped(window);
    _AddOpenWindow(wrapped, *window);
    wrapped->show();

    wrapped->ShowCollection(collection);
}

void DualView::OpenAddToFolder(std::shared_ptr<Collection> collection)
{
    AssertIfNotMainThread();

    auto window = std::make_shared<AddToFolder>(collection);
    _AddOpenWindow(window, *window);
    window->show();
}

void DualView::OpenAddToFolder(std::shared_ptr<Folder> folder)
{
    AssertIfNotMainThread();

    auto window = std::make_shared<AddToFolder>(folder);
    _AddOpenWindow(window, *window);
    window->show();
}

void DualView::OpenRemoveFromFolders(std::shared_ptr<Collection> collection)
{
    AssertIfNotMainThread();

    auto window = std::make_shared<RemoveFromFolders>(collection);
    _AddOpenWindow(window, *window);
    window->show();
}

void DualView::OpenRemoveFromFolders(std::shared_ptr<Folder> folder)
{
    AssertIfNotMainThread();

    auto window = std::make_shared<RemoveFromFolders>(folder);
    _AddOpenWindow(window, *window);
    window->show();
}

void DualView::OpenImporter()
{
    AssertIfNotMainThread();

    auto builder = Gtk::Builder::create_from_resource("/com/boostslair/dualviewpp/resources/gui/importer.glade");

    Importer* window;
    builder->get_widget_derived("FileImporter", window);

    if (!window)
    {
        LOG_ERROR("Importer window GUI layout is invalid");
        return;
    }

    LOG_INFO("Opened Importer window");

    std::shared_ptr<Importer> wrapped(window);
    _AddOpenWindow(wrapped, *window);
    wrapped->show();
}

void DualView::OpenMainWindow(bool present /*= true*/)
{
    AssertIfNotMainThread();

    if (MainMenu)
    {
        if (!MainMenu->is_visible())
            Application->add_window(*MainMenu);

        MainMenu->show();

        if (present)
            MainMenu->present();
    }
}

void DualView::OpenImporter(const std::vector<std::shared_ptr<Image>>& images)
{
    AssertIfNotMainThread();

    auto builder = Gtk::Builder::create_from_resource("/com/boostslair/dualviewpp/resources/gui/importer.glade");

    Importer* window;
    builder->get_widget_derived("FileImporter", window);

    if (!window)
    {
        LOG_ERROR("Importer window GUI layout is invalid");
        return;
    }

    window->AddExisting(images);

    LOG_INFO("Opened Importer window with images");

    std::shared_ptr<Importer> wrapped(window);
    _AddOpenWindow(wrapped, *window);
    wrapped->show();
}

void DualView::OpenDownloader()
{
    AssertIfNotMainThread();

    // Show it //
    Application->add_window(*_Downloader);
    _Downloader->show();
    _Downloader->present();
}

void DualView::OpenTagCreator(const std::string& settext)
{
    OpenTagCreator();

    _TagManager->SetCreateTag(settext);
}

void DualView::OpenTagInfo(const std::string& tagtext)
{
    OpenTagCreator();

    _TagManager->SetSearchString(tagtext);
}

void DualView::OpenTagCreator()
{
    AssertIfNotMainThread();

    // Show it //
    Application->add_window(*_TagManager);
    _TagManager->show();
    _TagManager->present();
}

void DualView::RunFolderCreatorAsDialog(
    const VirtualPath& path, const std::string& prefillnewname, Gtk::Window& parentwindow)
{
    AssertIfNotMainThread();

    bool isprivate = false;

    FolderCreator window(path, prefillnewname);

    window.set_transient_for(parentwindow);
    window.set_modal(true);

    int result = window.run();

    if (result != Gtk::RESPONSE_OK)
        return;

    std::string name;
    VirtualPath createpath;
    window.GetNewName(name, createpath);

    LOG_INFO("Trying to create new folder \"" + name + "\" at: " + createpath.operator std::string());

    try
    {
        auto parent = GetFolderFromPath(createpath);

        if (!parent)
            throw std::runtime_error("Parent folder at path doesn't exist");

        auto created = _Database->InsertFolder(name, isprivate, *parent);

        if (!created)
            throw std::runtime_error("Failed to create folder");
    }
    catch (const std::exception& e)
    {
        auto dialog = Gtk::MessageDialog(parentwindow,
            "Failed to create folder \"" + name + "\" at: " + createpath.operator std::string(), false,
            Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
        dialog.set_secondary_text("Try again without using special characters in the "
                                  "folder name. And verify that the path at which the folder is to be "
                                  "created is valid. Exception message: " +
            std::string(e.what()));
        dialog.run();
    }
}

void DualView::OpenImageFinder()
{
    AssertIfNotMainThread();

    auto builder = Gtk::Builder::create_from_resource("/com/boostslair/dualviewpp/resources/gui/image_finder.glade");

    ImageFinder* window;
    builder->get_widget_derived("FileFinder", window);

    if (!window)
    {
        LOG_ERROR("FileFinder window GUI layout is invalid");
        return;
    }

    std::shared_ptr<ImageFinder> wrapped(window);
    _AddOpenWindow(wrapped, *window);
    wrapped->show();
}

void DualView::OpenUndoWindow()
{
    AssertIfNotMainThread();

    auto existing = _UndoWindow.lock();

    if (existing)
    {
        existing->show();
        return;
    }

    auto window = std::make_shared<UndoWindow>();
    _AddOpenWindow(window, *window);
    window->show();

    _UndoWindow = window;
}

void DualView::OpenAlreadyImportedDeleteWindow(const std::string& initialPath)
{
    AssertIfNotMainThread();

    Application->add_window(*_AlreadyImportedImageDeleter);
    _AlreadyImportedImageDeleter->show();
    _AlreadyImportedImageDeleter->present();

    if (_AlreadyImportedImageDeleter->IsRunning())
    {
        LOG_INFO("Already imported deleter is in use already");
        return;
    }

    if (!initialPath.empty() && boost::filesystem::exists(initialPath))
    {
        LOG_INFO("Automatically starting already imported delete with path: " + initialPath);
        _AlreadyImportedImageDeleter->SetSelectedFolder(initialPath);
        _AlreadyImportedImageDeleter->Start();
    }
}

void DualView::OpenDuplicateFinder()
{
    AssertIfNotMainThread();

    auto existing = _DuplicateFinderWindow.lock();

    if (existing)
    {
        existing->show();
        return;
    }

    auto window = std::make_shared<DuplicateFinderWindow>();
    _AddOpenWindow(window, *window);
    window->show();

    _DuplicateFinderWindow = window;
}

void DualView::OpenReorder(const std::shared_ptr<Collection>& collection)
{
    if (!collection)
        return;

    AssertIfNotMainThread();

    auto window = std::make_shared<ReorderWindow>(collection);
    _AddOpenWindow(window, *window);
    window->show();
}

void DualView::OpenCollectionRename(
    const std::shared_ptr<Collection>& collection, Gtk::Window* parentWindow /*= nullptr*/)
{
    if (!collection)
        return;

    AssertIfNotMainThread();

    auto window = std::make_shared<RenameWindow>(
        collection->GetName(),
        [this, collection](const std::string& newName)
        {
            if (!collection->Rename(newName))
            {
                return std::make_tuple<bool, std::string>(false, "Collection rename failed. Check logs for SQL errors");
            }

            return std::make_tuple<bool, std::string>(true, "");
        },
        [this, collection](const std::string& potentialName)
        {
            if (potentialName.empty())
            {
                return std::make_tuple<bool, std::string>(false, "Name can't be empty");
            }

            if (_Database->CheckIsCollectionNameInUseAG(potentialName, collection->GetID()))
            {
                return std::make_tuple<bool, std::string>(false, "Name is already in-use");
            }

            return std::make_tuple<bool, std::string>(true, "");
        });

    _AddOpenWindow(window, *window);
    if (parentWindow)
        window->set_transient_for(*parentWindow);
    window->show();
}

void DualView::OpenFolderRename(const std::shared_ptr<Folder>& folder, Gtk::Window* parentWindow /*= nullptr*/)
{
    if (!folder)
        return;

    AssertIfNotMainThread();

    auto window = std::make_shared<RenameWindow>(
        folder->GetName(),
        [this, folder](const std::string& newName)
        {
            if (!folder->Rename(newName))
            {
                return std::make_tuple<bool, std::string>(false, "Folder rename failed. Check logs for errors");
            }

            return std::make_tuple<bool, std::string>(true, "");
        },
        [this, folder](const std::string& potentialName)
        {
            if (potentialName.empty())
            {
                return std::make_tuple<bool, std::string>(false, "Name can't be empty");
            }

            const auto conflictIn = _Database->SelectFirstParentFolderWithChildFolderNamedAG(*folder, potentialName);

            if (conflictIn)
            {
                return std::make_tuple<bool, std::string>(
                    false, "New name already exists in folder '" + conflictIn->GetName() + "'");
            }

            return std::make_tuple<bool, std::string>(true, "");
        });

    _AddOpenWindow(window, *window);
    if (parentWindow)
        window->set_transient_for(*parentWindow);
    window->show();
}

void DualView::OpenActionEdit(const std::shared_ptr<ImageMergeAction>& action)
{
    if (!action)
        return;

    AssertIfNotMainThread();

    auto window = std::make_shared<MergeActionEditor>(action);
    _AddOpenWindow(window, *window);
    window->show();
}

void DualView::OpenDownloadItemEditor(const std::shared_ptr<NetGallery>& download)
{
    if (!download)
        return;

    AssertIfNotMainThread();

    auto builder =
        Gtk::Builder::create_from_resource("/com/boostslair/dualviewpp/resources/gui/download_item_editor.glade");

    DownloadItemEditor* window;
    builder->get_widget_derived("DownloadItemEditor", window, download);

    if (!window)
    {
        LOG_ERROR("DownloadItemEditor window GUI layout is invalid");
        return;
    }

    std::shared_ptr<DownloadItemEditor> wrapped(window);
    _AddOpenWindow(wrapped, *window);
    wrapped->show();
}

// ------------------------------------ //
void DualView::OnNewImageLinkReceived(const std::string& url, const std::string& referrer)
{
    AssertIfNotMainThread();

    // TODO: support passing cookies through this method (cookies should be deleted after the download is done)

    // Find a downloader that can be used
    std::shared_ptr<DownloadSetup> existing;
    bool openednew = false;

    while (true)
    {
        for (const auto& dlsetup : OpenDLSetups)
        {
            // Check is it good for us //
            auto locked = dlsetup.lock();

            if (!locked)
                continue;

            if (!locked->IsValidTargetForImageAdd())
                continue;

            // Found a good one //
            existing = locked;
            break;
        }

        if (existing)
        {
            // We don't need a new window //
            existing->AddExternallyFoundLinkRaw(url, referrer);
            return;
        }

        if (openednew)
        {
            LOG_ERROR("Failed to find a suitable DownloadSetup even after opening a new one");
            return;
        }

        openednew = true;

        // Open a new window and try again //
        OpenDownloadSetup(false);
    }
}

void DualView::OnNewGalleryLinkReceived(const std::string& url)
{
    AssertIfNotMainThread();

    auto scanner = _PluginManager->GetScannerForURL(url);

    if (scanner)
    {
        const auto handledUrl = DownloadSetup::HandleCanonization(url, *scanner);

        if (scanner->IsUrlNotGallery(handledUrl))
        {
            LOG_INFO("New gallery link is actually a content page");
            OnNewImagePageLinkReceived(url);
            return;
        }
    }

    // Find a downloader that can be used
    std::shared_ptr<DownloadSetup> existing;
    bool openednew = false;

    while (true)
    {
        for (const auto& dlsetup : OpenDLSetups)
        {
            // Check is it good for us //
            auto locked = dlsetup.lock();

            if (!locked)
                continue;

            if (!locked->IsValidForNewPageScan())
                continue;

            // Found a good one //
            existing = locked;
            break;
        }

        if (existing)
        {
            // We don't need a new window //
            existing->SetNewUrlToDl(url);
            return;
        }

        if (openednew)
        {
            LOG_ERROR("Failed to find a suitable DownloadSetup even after opening a new one");
            return;
        }

        openednew = true;

        // Open a new window and try again //
        OpenDownloadSetup(false, true);
    }
}

void DualView::OnNewImagePageLinkReceived(const std::string& url)
{
    // Find a downloader that can be used
    std::shared_ptr<DownloadSetup> existing;
    bool openednew = false;

    while (true)
    {
        for (const auto& dlsetup : OpenDLSetups)
        {
            // Check is it good for us //
            auto locked = dlsetup.lock();

            if (!locked)
                continue;

            if (!locked->IsValidTargetForScanLink())
                continue;

            // Found a good one //
            existing = locked;
            break;
        }

        if (existing)
        {
            // We don't need a new window //
            existing->AddExternalScanLinkRaw(url);
            return;
        }

        if (openednew)
        {
            LOG_ERROR("Failed to find a suitable DownloadSetup even after opening a new one");
            return;
        }

        openednew = true;

        // Open a new window and try again //
        OpenDownloadSetup(false, true);
    }
}

std::shared_ptr<DownloadSetup> DualView::OpenDownloadSetup(bool useropened /*= true*/, bool capture /*= false*/)
{
    LEVIATHAN_ASSERT(!(useropened && capture), "both useropened and capture may not be true");

    AssertIfNotMainThread();

    auto builder = Gtk::Builder::create_from_resource("/com/boostslair/dualviewpp/resources/gui/download_setup.glade");

    DownloadSetup* window;
    builder->get_widget_derived("DownloadSetup", window);

    if (!window)
    {
        LOG_ERROR("DownloadSetup window GUI layout is invalid");
        return nullptr;
    }

    LOG_INFO("Opened DownloadSetup window");

    std::shared_ptr<DownloadSetup> wrapped(window);
    _AddOpenWindow(wrapped, *window);
    OpenDLSetups.push_back(wrapped);

    if (useropened)
        wrapped->DisableAddActive();

    wrapped->show();

    if (capture)
        wrapped->EnableAddActive();

    return wrapped;
}

// ------------------------------------ //
void DualView::RegisterWindow(Gtk::Window& window)
{
    Application->add_window(window);
}

void DualView::WindowClosed(std::shared_ptr<WindowClosedEvent> event)
{
    {
        std::lock_guard<std::mutex> lock(MessageQueueMutex);

        CloseEvents.push_back(event);
    }

    MessageDispatcher.emit();
}

void DualView::_AddOpenWindow(std::shared_ptr<BaseWindow> window, Gtk::Window& gtk)
{
    AssertIfNotMainThread();

    OpenWindows.push_back(window);
    RegisterWindow(gtk);
}

// ------------------------------------ //
std::string DualView::GetThumbnailFolder() const
{
    return (boost::filesystem::path(GetSettings().GetPrivateCollection()) / boost::filesystem::path("thumbnails/"))
        .c_str();
}

// ------------------------------------ //
// Database saving functions
bool DualView::AddToCollection(std::vector<std::shared_ptr<Image>> resources, bool move, std::string collectionname,
    const TagCollection& addcollectiontags, std::function<void(float)> progresscallback /*= nullptr*/)
{
    // Make sure ready to add //
    for (const auto& img : resources)
    {
        if (!img->IsReady() || !img->GetIsValid())
        {
            LOG_WARNING("Cannot import because at least one image isn't ready or valid");
            return false;
        }
    }

    // Trim whitespace //
    Leviathan::StringOperations::RemovePreceedingTrailingSpaces(collectionname);

    std::shared_ptr<Collection> addtocollection;

    bool canapplytags = true;

    auto uncategorized = GetUncategorized();

    // No collection specified, get Uncategorized //
    if (collectionname.empty())
    {
        addtocollection = uncategorized;
        canapplytags = false;
    }
    else
    {
        addtocollection = GetOrCreateCollection(collectionname, IsInPrivateMode);

        if (!addtocollection)
            throw Leviathan::InvalidArgument("Invalid collection name");
    }

    LEVIATHAN_ASSERT(addtocollection, "Failed to get collection object");

    std::vector<std::string> filestodelete;

    {
        GUARD_LOCK_OTHER(_Database);

        auto transaction = std::make_unique<DoDBTransaction>(*_Database, guard);

        if (canapplytags)
            addtocollection->AddTags(addcollectiontags, guard);

        size_t currentitem = 0;
        const auto maxitems = resources.size();

        auto order = addtocollection->GetLastShowOrder(guard);

        // Some lambdas to run after success. used to simplify this
        std::vector<std::function<void()>> onSuccessFunctions;

        // Save resources to database if not loaded from the database //

        for (auto& resource : resources)
        {
            std::shared_ptr<Image> actualresource;

            if (!resource->IsInDatabase())
            {
                // If the image hash is in the collection then we shouldn't be here //
                // But just in case we should check to make absolutely sure
                auto existingimage = _Database->SelectImageByHash(guard, resource->GetHash());

                if (existingimage)
                {
                    LOG_WARNING("Trying to import a duplicate hash image");

                    if (existingimage->IsDeleted())
                    {
                    }

                    // Delete original file if moving //
                    if (move)
                    {
                        const auto path = resource->GetResourcePath();

                        onSuccessFunctions.push_back(
                            [path]()
                            {
                                LOG_INFO("Deleting moved (duplicate) file: " + path);

                                boost::filesystem::remove(path);
                            });
                    }

                    if (resource->GetTags()->HasTags(guard))
                        existingimage->GetTags()->Add(*resource->GetTags(), guard);

                    actualresource = existingimage;
                }
                else
                {
                    // TODO: unmove if fails to add
                    if (!MoveFileToCollectionFolder(resource, addtocollection, move))
                    {
                        LOG_ERROR("Failed to move file to collection's folder");
                        return false;
                    }

                    std::shared_ptr<TagCollection> tagstoapply;

                    // Store tags for applying //
                    if (resource->GetTags()->HasTags(guard))
                        tagstoapply = resource->GetTags();

                    try
                    {
                        _Database->InsertImage(guard, *resource);
                    }
                    catch (const InvalidSQL& e)
                    {
                        // We have already moved the image so this is a problem
                        LOG_INFO("TODO: move file back after adding fails");
                        LOG_ERROR("Sql error adding image to collection: ");
                        e.PrintToLog();
                        return false;
                    }

                    // Apply tags //
                    if (tagstoapply)
                        resource->GetTags()->Add(*tagstoapply, guard);

                    actualresource = resource;
                }
            }
            else
            {
                actualresource = resource;

                // Remove from uncategorized if not adding to that //
                if (addtocollection != uncategorized)
                {
                    uncategorized->RemoveImage(actualresource, guard);
                }
            }

            LEVIATHAN_ASSERT(actualresource, "actualresource not set in DualView import image");

            // Associate with collection //
            addtocollection->AddImage(actualresource, ++order, guard);

            currentitem++;

            if (progresscallback)
                progresscallback(currentitem / (float)maxitems);
        }

        // Commit transaction //
        // by reseting the smart pointer
        transaction.reset();

        for (const auto& func : onSuccessFunctions)
        {
            func();
        }

        // We no longer use the database
    }

    // These are duplicate files of already existing ones
    bool exists = false;

    do
    {
        exists = false;

        for (const std::string& file : filestodelete)
        {
            if (boost::filesystem::exists(file))
            {
                exists = true;

                try
                {
                    boost::filesystem::remove(file);
                }
                catch (const boost::filesystem::filesystem_error&)
                {
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

    } while (exists);

    return true;
}

// ------------------------------------ //
std::shared_ptr<Collection> DualView::GetOrCreateCollection(const std::string& name, bool isprivate)
{
    auto existing = _Database->SelectCollectionByNameAG(name);

    if (existing)
        return existing;

    return _Database->InsertCollectionAG(name, isprivate);
}

void DualView::AddCollectionToFolder(
    std::shared_ptr<Folder> folder, std::shared_ptr<Collection> collection, bool removefromroot /*= true*/)
{
    if (!folder || !collection)
        return;

    _Database->InsertCollectionToFolderAG(*folder, *collection);

    if (!folder->IsRoot() && removefromroot)
    {
        _Database->DeleteCollectionFromRootIfInAnotherFolder(*collection);
    }
}

void DualView::RemoveCollectionFromFolder(std::shared_ptr<Collection> collection, std::shared_ptr<Folder> folder)
{
    if (!folder || !collection)
        return;

    GUARD_LOCK_OTHER(_Database);

    _Database->DeleteCollectionFromFolder(guard, *folder, *collection);

    // Make sure the Collection is in at least one folder //
    _Database->InsertCollectionToRootIfInNone(guard, *collection);
}

// ------------------------------------ //
// Database load functions
std::shared_ptr<Folder> DualView::GetRootFolder()
{
    if (RootFolder)
        return RootFolder;

    LEVIATHAN_ASSERT(_Database, "Trying to GetRootFolder before database is opened");

    RootFolder = _Database->SelectRootFolderAG();
    return RootFolder;
}

std::shared_ptr<Collection> DualView::GetUncategorized()
{
    if (UncategorizedCollection)
        return UncategorizedCollection;

    LEVIATHAN_ASSERT(_Database, "Trying to GetUncategorized before database is opened");

    UncategorizedCollection = _Database->SelectCollectionByNameAG("Uncategorized");
    return UncategorizedCollection;
}

// ------------------------------------ //
std::vector<std::string> DualView::GetFoldersCollectionIsIn(std::shared_ptr<Collection> collection)
{
    std::vector<std::string> result;

    if (!collection)
        return result;

    const auto folderids = _Database->SelectFoldersCollectionIsIn(*collection);

    for (auto id : folderids)
    {
        result.push_back(ResolvePathToFolder(id));
    }

    return result;
}

std::vector<std::string> DualView::GetFoldersFolderIsIn(std::shared_ptr<Folder> folder)
{
    std::vector<std::string> result;

    if (!folder)
        return result;

    const auto folderIds = _Database->SelectFolderParents(*folder);

    for (auto id : folderIds)
    {
        result.push_back(ResolvePathToFolder(id));
    }

    return result;
}

// ------------------------------------ //
std::shared_ptr<Folder> DualView::GetFolderFromPath(const VirtualPath& path)
{
    // Root folder //
    if (path.IsRootPath())
        return GetRootFolder();

    // Loop through all the path components and verify that a folder exists

    std::shared_ptr<Folder> currentfolder;

    for (auto iter = path.begin(); iter != path.end(); ++iter)
    {
        auto part = *iter;

        if (part.empty())
        {
            // String ended //
            return currentfolder;
        }

        if (!currentfolder && (part == "Root"))
        {
            currentfolder = GetRootFolder();
            continue;
        }

        if (!currentfolder)
        {
            // Didn't begin with root //
            return nullptr;
        }

        // Find a folder with the current name inside currentfolder //
        auto nextfolder = _Database->SelectFolderByNameAndParentAG(part, *currentfolder);

        if (!nextfolder)
        {
            // There's a nonexistant folder in the path
            return nullptr;
        }

        // Moved to the next part
        currentfolder = nextfolder;
    }

    return currentfolder;
}

//! Prevents ResolvePathHelperRecursive from going through infinite loops
struct DV::ResolvePathInfinityBlocker
{
    std::vector<DBID> EarlierFolders;
};

std::tuple<bool, VirtualPath> DualView::ResolvePathHelperRecursive(
    DBID currentid, const VirtualPath& currentpath, const ResolvePathInfinityBlocker& earlieritems)
{
    auto currentfolder = _Database->SelectFolderByIDAG(currentid);

    if (!currentfolder)
        return std::make_tuple(false, currentpath);

    if (currentfolder->IsRoot())
    {
        return std::make_tuple(true, VirtualPath() / currentpath);
    }

    ResolvePathInfinityBlocker infinity;
    infinity.EarlierFolders = earlieritems.EarlierFolders;
    infinity.EarlierFolders.push_back(currentid);

    for (auto parentid : _Database->SelectFolderParents(*currentfolder))
    {
        // Skip already seen folders //
        bool found = false;

        for (auto alreadychecked : infinity.EarlierFolders)
        {
            if (alreadychecked == parentid)
            {
                found = true;
                break;
            }
        }

        if (found)
            continue;

        const auto result =
            ResolvePathHelperRecursive(parentid, VirtualPath(currentfolder->GetName()) / currentpath, infinity);

        if (std::get<0>(result))
        {
            // Found a full path //
            return result;
        }
    }

    // Didn't find a full path //
    return std::make_tuple(false, currentpath);
}

VirtualPath DualView::ResolvePathToFolder(DBID id)
{
    ResolvePathInfinityBlocker infinityblocker;

    const auto result = ResolvePathHelperRecursive(id, VirtualPath(""), infinityblocker);

    if (!std::get<0>(result))
    {
        // Failed //
        return "Recursive Path: " + static_cast<std::string>(std::get<1>(result));
    }

    return std::get<1>(result);
}

// ------------------------------------ //
std::shared_ptr<AppliedTag> DualView::ParseTagWithBreakRule(const std::string& str) const
{
    auto rule = _Database->SelectTagBreakRuleByStr(str);

    if (!rule)
        return nullptr;

    std::string tagname;
    std::shared_ptr<Tag> tag;
    auto modifiers = rule->DoBreak(str, tagname, tag);

    if (!tag && modifiers.empty())
    {
        // Rule didn't match //
        return nullptr;
    }

    return std::make_shared<AppliedTag>(tag, modifiers);
}

std::string DualView::GetExpandedTagFromSuperAlias(const std::string& str) const
{
    return _Database->SelectTagSuperAlias(str);
}

std::shared_ptr<AppliedTag> DualView::ParseTagName(const std::string& str) const
{
    auto byname = _Database->SelectTagByNameOrAlias(str);

    if (byname)
        return std::make_shared<AppliedTag>(byname);

    // Try to get expanded form //
    const auto expanded = GetExpandedTagFromSuperAlias(str);

    if (!expanded.empty())
    {
        // Parse the replacement tag //
        return ParseTagFromString(expanded);
    }

    return nullptr;
}

std::tuple<std::shared_ptr<AppliedTag>, std::string, std::shared_ptr<AppliedTag>> DualView::ParseTagWithComposite(
    const std::string& str) const
{
    // First split it into words //
    std::vector<std::string> words;
    Leviathan::StringOperations::CutString<std::string>(str, " ", words);

    // Must be 3 words or more
    if (words.size() < 3)
        return std::make_tuple(nullptr, "", nullptr);

    // Find a middle word for which the following works
    // left side can be parsed with ParseTagWithOnlyModifiers middle word is a word like
    // "on", "with", or anything really and the right side can also be parsed with
    // ParseTagWithOnlyModifiers

    std::vector<std::string*> left;
    std::vector<std::string*> right;

    for (size_t i = 1; i < words.size() - 1; ++i)
    {
        // Build the words //
        size_t insertIndex = 0;
        left.resize(i);

        for (size_t a = 0; a < i; ++a)
        {
            left[insertIndex++] = &words[a];
        }

        std::shared_ptr<AppliedTag> parsedleft;
        std::shared_ptr<AppliedTag> parsedright;

        try
        {
            parsedleft = ParseTagFromString(Leviathan::StringOperations::StitchTogether<std::string>(left, " "));
        }
        catch (const Leviathan::InvalidArgument&)
        {
            // No such tag //
            continue;
        }

        if (!parsedleft)
            continue;

        insertIndex = 0;
        right.resize(words.size() - 1 - i);

        for (size_t a = i + 1; a < words.size(); ++a)
        {
            right[insertIndex++] = &words[a];
        }

        try
        {
            parsedright = ParseTagFromString(Leviathan::StringOperations::StitchTogether<std::string>(right, " "));
        }
        catch (const Leviathan::InvalidArgument&)
        {
            // No such tag //
            continue;
        }

        if (!parsedright)
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
    const std::shared_ptr<AppliedTag>& maintag, const std::vector<std::string*>& words, bool taglast) const
{
    // All the words need to be modifiers or a break rule must accept them //
    bool success = true;
    std::vector<std::shared_ptr<TagModifier>> modifiers;
    modifiers.reserve(words.size());

    for (size_t a = 0; a < words.size(); ++a)
    {
        auto mod = _Database->SelectTagModifierByNameOrAliasAG(*words[a]);

        if (mod)
        {
            modifiers.push_back(mod);
            continue;
        }

        // This wasn't a modifier //
        auto broken = ParseTagWithBreakRule(*words[a]);

        // Must have parsed a tag that has only modifiers
        if (!broken || broken->GetTag() || broken->GetModifiers().empty())
        {
            // unknown modifier and no break rule //
            return nullptr;
        }

        // Modifiers from break rule //
        for (const auto& alreadyset : broken->GetModifiers())
        {
            modifiers.push_back(alreadyset);
        }
    }

    for (const auto& alreadyset : maintag->GetModifiers())
    {
        modifiers.push_back(alreadyset);
    }

    return std::make_shared<AppliedTag>(maintag->GetTag(), modifiers);
}

std::shared_ptr<AppliedTag> DualView::ParseTagWithOnlyModifiers(const std::string& str) const
{
    // First split it into words //
    std::vector<std::string> words;
    Leviathan::StringOperations::CutString<std::string>(str, " ", words);

    // Must be more than 1 word
    if (words.size() < 2)
        return nullptr;

    // Then try to match all the stuff in front of the tag into modifiers and the
    // last word(s) into a tag
    std::vector<std::string*> back;
    std::vector<std::string*> front;

    for (size_t i = 0; i < words.size(); ++i)
    {
        // Build the back string with the first excluded word being at words[i]
        size_t insertIndex = 0;
        back.resize(words.size() - 1 - i);

        for (size_t a = i + 1; a < words.size(); ++a)
        {
            back[insertIndex++] = &words[a];
        }

        front.resize(i + 1);
        insertIndex = 0;

        for (size_t a = 0; a <= i; ++a)
        {
            front[insertIndex++] = &words[a];
        }

        // Create strings //
        auto backStr = Leviathan::StringOperations::StitchTogether<std::string>(back, " ");

        // Back needs to be a tag //
        auto tag = ParseTagName(backStr);

        if (!tag)
        {
            // Maybe tag is first //
            auto frontStr = Leviathan::StringOperations::StitchTogether<std::string>(front, " ");

            tag = ParseTagName(frontStr);

            if (!tag)
                continue;

            // Try parsing it //
            auto finished = ParseHelperCheckModifiersAndBreakRules(tag, back, false);

            if (finished)
                return finished;

            continue;
        }

        auto finished = ParseHelperCheckModifiersAndBreakRules(tag, front, false);

        if (finished)
        {
            // It succeeded //
            return finished;
        }
    }

    return nullptr;
}

std::shared_ptr<AppliedTag> DualView::ParseTagFromString(std::string str) const
{
    // TODO: make all the fixes here recursively call this to be more exhaustive
    // Strip whitespace //
    Leviathan::StringOperations::RemovePreceedingTrailingSpaces(str);

    if (str.empty())
        return nullptr;

    // Convert to lower //
    str = StringToLower(str);

    // Exact match a tag //
    auto existingtag = ParseTagName(str);

    if (existingtag)
        return existingtag;

    // Wasn't exactly a single tag //

    // Check does removing whitespace create some existing tag
    // TODO: also check if converting underscores to spaces helps
    auto nowhitespace = Leviathan::StringOperations::RemoveCharacters<std::string>(str, " ");

    if (nowhitespace.size() != str.size())
    {
        existingtag = ParseTagName(nowhitespace);

        if (existingtag)
            return existingtag;
    }

    // Modifier before tag //
    auto modifiedtag = ParseTagWithOnlyModifiers(str);

    if (modifiedtag)
        return modifiedtag;

    // Detect a composite //
    std::vector<std::string> words;
    Leviathan::StringOperations::CutString<std::string>(str, " ", words);

    if (!words.empty())
    {
        // Try to break composite //
        auto composite = ParseTagWithComposite(str);

        if (std::get<0>(composite))
        {
            return std::get<0>(composite);
        }
    }

    // Break rules are detected by ParseTagName

    // Check does removing ending 's' create an existing tag
    if (str.back() == 's' && str.size() > 1)
    {
        try
        {
            return ParseTagFromString(str.substr(0, str.size() - 1));
        }
        catch (const Leviathan::InvalidArgument& e)
        {
            // It didn't
            throw Leviathan::InvalidArgument("unknown tag '" + str + "'");
        }
    }

    throw Leviathan::InvalidArgument("unknown tag '" + str + "'");
}

// ------------------------------------ //
// #define GETSUGGESTIONS_DEBUG

#ifdef GETSUGGESTIONS_DEBUG

#define SUGG_DEBUG(x) LOG_WRITE("Suggestions " + std::string(x));

#else
#define SUGG_DEBUG(x)                                                                                                  \
    {                                                                                                                  \
    }

#endif

std::vector<std::string> DualView::GetSuggestionsForTag(std::string str, size_t maxcount /*= 100*/) const
{
    // Strip whitespace //
    Leviathan::StringOperations::RemovePreceedingTrailingSpaces(str);

    if (str.empty())
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
    while (!itr.IsOutOfBounds())
    {
        auto currentword = itr.GetUntilNextCharacterOrAll<std::string>(' ');

        if (!currentword)
            continue;

        currentpart += currentpart.empty() ? *currentword : " " + *currentword;

        if (false)
        {
thingwasvalidlabel:
            SUGG_DEBUG("Part was valid: " + currentpart);
            prefix += prefix.empty() ? currentpart : " " + currentpart;
            currentpart.clear();
            continue;
        }

        if (tagallowed)
        {
            auto byname = _Database->SelectTagByNameOrAlias(currentpart);
            if (byname)
            {
                tagallowed = false;
                modifierallowed = false;
                goto thingwasvalidlabel;
            }
        }

        if (modifierallowed)
        {
            auto mod = _Database->SelectTagModifierByNameOrAliasAG(currentpart);
            if (mod)
            {
                modifierallowed = false;
                tagallowed = true;
                goto thingwasvalidlabel;
            }
        }

        auto rule = _Database->SelectTagBreakRuleByStr(currentpart);

        if (rule)
        {
            std::string tagname;
            std::shared_ptr<Tag> tag;
            auto modifiers = rule->DoBreak(currentpart, tagname, tag);

            if (tag || !modifiers.empty())
            {
                // Rule matched
                modifierallowed = false;
                tagallowed = true;
                goto thingwasvalidlabel;
            }
        }

        auto alias = _Database->SelectTagSuperAlias(currentpart);
        if (!alias.empty())
        {
            modifierallowed = false;
            tagallowed = true;
            goto thingwasvalidlabel;
        }

        // TODO: check combined with
    }

    // Add space after prefix //
    if (!prefix.empty())
        prefix += " ";

    SUGG_DEBUG("Finished parsing and valid prefix is: " + prefix);
    SUGG_DEBUG("Unparsed part is: " + currentpart);

    // If there's nothing left in currentpart the tag would be successfully parsed
    // So just make sure that it is and add it to the result
    if (currentpart.empty())
    {
        try
        {
            auto parsed = ParseTagFromString(str);

            if (parsed)
                result.push_back(str);
        }
        catch (...)
        {
            // Not actually a tag
            LOG_WARNING("Get suggestions thought \"" + str +
                "\" would be a valid tag "
                "but it isn't");
        }

        // Also we want to get longer tags that start with the same thing //
        RetrieveTagsMatching(result, str);
    }
    else
    {
        // This is used to block generating combine suggestions where the word from which the
        // further suggestions are generated was actually part of a multiword tag
        // TODO: find a better way to do this
        bool foundExactPrefix = false;

        // Get suggestions for it //
        {
            SUGG_DEBUG("Finding suggestions: " + currentpart);

            std::vector<std::string> tmpholder;
            RetrieveTagsMatching(tmpholder, currentpart);

            result.reserve(result.size() + tmpholder.size());

            for (const auto& gotmatch : tmpholder)
            {
                if (!foundExactPrefix)
                {
                    if (gotmatch.find(currentpart) == 0)
                    {
                        SUGG_DEBUG("Found exact prefix: " + gotmatch + ", currentpart: " + currentpart);
                        foundExactPrefix = true;
                    }
                }

                SUGG_DEBUG("Found suggestion: " + gotmatch + ", prefix: " + prefix);
                result.push_back(prefix + gotmatch);
            }
        }

        // Combines //
        const auto tail = Leviathan::StringOperations::RemoveFirstWords(currentpart, 1);

        if (tail != currentpart && !foundExactPrefix)
        {
            SUGG_DEBUG("Finding combine suggestions: " + tail);

            const auto tailprefix = prefix + currentpart.substr(0, currentpart.size() - tail.size());

            std::vector<std::string> tmpholder =
                GetSuggestionsForTag(tail, std::max(maxcount - result.size(), maxcount / 4));

            result.reserve(result.size() + tmpholder.size());

            for (const auto& gotmatch : tmpholder)
            {
                SUGG_DEBUG("Found combine suggestion: " + gotmatch + ", prefix: " + tailprefix);
                result.push_back(tailprefix + gotmatch);
            }
        }
        else
        {
        }
    }

    // Sort the most relevant results first //
    DV::SortSuggestions(result.begin(), result.end(), str);

    result.erase(std::unique(result.begin(), result.end()), result.end());

    if (result.size() > maxcount)
        result.resize(maxcount);

    SUGG_DEBUG("Resulting suggestions: " + Convert::ToString(result.size()));
    for (auto iter = result.begin(); iter != result.end(); ++iter)
        SUGG_DEBUG(*iter);

    return result;
}

// ------------------------------------ //
void DualView::RetrieveTagsMatching(std::vector<std::string>& result, const std::string& str) const
{
    _Database->SelectTagNamesWildcard(result, str);

    _Database->SelectTagAliasesWildcard(result, str);

    _Database->SelectTagModifierNamesWildcard(result, str);

    _Database->SelectTagBreakRulesByStrWildcard(result, str);

    _Database->SelectTagSuperAliasWildcard(result, str);
}

// ------------------------------------ //
// Gtk callbacks
void DualView::OpenImageFile_OnClick()
{
    Gtk::FileChooserDialog dialog("Choose an image to open", Gtk::FILE_CHOOSER_ACTION_OPEN);
    dialog.set_transient_for(*MainMenu);

    // Add response buttons the the dialog:
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Open", Gtk::RESPONSE_OK);

    // Add filters, so that only certain file types can be selected:
    auto filter_image = Gtk::FileFilter::create();
    filter_image->set_name("Image Files");

    for (const auto& type : SUPPORTED_EXTENSIONS)
    {
        filter_image->add_mime_type(std::get<1>(type));
    }
    dialog.add_filter(filter_image);

    auto filter_any = Gtk::FileFilter::create();
    filter_any->set_name("Any files");
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);

    // Wait for a selection
    int result = dialog.run();

    // Handle the response:
    switch (result)
    {
        case (Gtk::RESPONSE_OK):
        {
            std::string filename = dialog.get_filename();

            if (filename.empty())
                return;

            OpenImageViewer(filename);
            return;
        }
        case (Gtk::RESPONSE_CANCEL):
        default:
        {
            // Canceled / nothing selected //
            return;
        }
    }
}

void DualView::OpenCollection_OnClick()
{
    // Show it //
    Application->add_window(*_CollectionView);
    _CollectionView->show();
    _CollectionView->present();
}

void DualView::OpenDebug_OnClick()
{
    Application->add_window(*_DebugWindow);
    _DebugWindow->show();
    _DebugWindow->present();
}

void DualView::OpenUndoWindow_OnClick()
{
    OpenUndoWindow();

    auto window = _UndoWindow.lock();

    if (window)
        window->present();
}

void DualView::OpenDuplicateFinder_OnClick()
{
    OpenDuplicateFinder();

    auto window = _DuplicateFinderWindow.lock();

    if (window)
        window->present();
}

void DualView::OpenMaintenance_OnClick()
{
    Application->add_window(*_MaintenanceTools);
    _MaintenanceTools->show();
    _MaintenanceTools->present();
}

// ------------------------------------ //
std::string DualView::MakePathUniqueAndShort(const std::string& path, bool allowCuttingFolder)
{
    const auto original = boost::filesystem::path(path);

    // First cut it //
    const auto length = boost::filesystem::absolute(original).string().size();

    const auto extension = original.extension();
    const auto baseFolder = original.parent_path();
    const auto fileName = original.stem();

    if (length > DUALVIEW_MAX_ALLOWED_PATH)
    {
        const std::string name = fileName.string();

        // Cut the folder name in half if the filename is already short
        if (name.length() < 12 && allowCuttingFolder)
        {
            const auto higherFolder = baseFolder.parent_path();
            const auto toCut = baseFolder.stem().string();

            if (toCut.length() > 10)
            {
                for (size_t cutPoint = toCut.size() / 2; cutPoint > 0; --cutPoint)
                {
                    // Cutting may result in invalid utf8 sequences
                    try
                    {
                        const std::string cutFolder = toCut.substr(0, cutPoint);
                        return MakePathUniqueAndShort(
                            (higherFolder / cutFolder / (name + extension.string())).string(), true);
                    }
                    catch (const boost::filesystem::filesystem_error& e)
                    {
                        LOG_WARNING(std::string("MakePathUniqueAndShort: folder cutting resulted in invalid string, "
                                                "exception: ") +
                            e.what());
                    }
                }
            }
            else
            {
                LOG_WARNING("Folder name to cut is too short to cut");
            }
        }

        for (size_t cutPoint = name.size() / 2; cutPoint > 0; --cutPoint)
        {
            // Cutting may result in invalid utf8 sequences
            try
            {
                const std::string newName = name.substr(0, cutPoint);
                return MakePathUniqueAndShort((baseFolder / (newName + extension.string())).string(), allowCuttingFolder);
            }
            catch (const boost::filesystem::filesystem_error& e)
            {
                LOG_WARNING(std::string("MakePathUniqueAndShort: filename cutting "
                                        "resulted in invalid string, exception: ") +
                    e.what());
            }
        }

        // Failed to cut //
        LOG_FATAL("MakePathUniqueAndShort: file name cutting failed to find valid filename, "
                  "start string: " +
            fileName.string());
        // We could not do a fatal here, as this should probably work
        // NO longer allows cutting the folder as that should have happened before trying to make the name shorter
        return MakePathUniqueAndShort((baseFolder / ("a" + extension.string())).string(), false);
    }

    // Then make sure it doesn't exist //
    if (!boost::filesystem::exists(original))
    {
        return original.c_str();
    }

    long number = 0;

    boost::filesystem::path finaltarget;

    do
    {
        finaltarget = baseFolder / (fileName.string() + "_" + Convert::ToString(++number) + extension.string());

    } while (boost::filesystem::exists(finaltarget));

    // Make sure it is still short enough
    return MakePathUniqueAndShort(finaltarget.string(), allowCuttingFolder);
}

std::string DualView::CalculateBase64EncodedHash(const std::string& str)
{
    // Calculate sha256 hash //
    unsigned char digest[CryptoPP::SHA256::DIGESTSIZE];

    CryptoPP::SHA256().CalculateDigest(digest, reinterpret_cast<const unsigned char*>(str.data()), str.length());

    static_assert(sizeof(digest) == CryptoPP::SHA256::DIGESTSIZE, "sizeof funkyness");

    // Encode it //
    std::string hash = base64_encode(digest, sizeof(digest));

    // Make it path safe //
    return Leviathan::StringOperations::ReplaceSingleCharacter<std::string>(hash, '/', '_');
}
