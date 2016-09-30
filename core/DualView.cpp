// ------------------------------------ //
#include "DualView.h"
#include "Common.h"


#include "windows/SingleView.h"
#include "core/CacheManager.h"

#include "Settings.h"
#include "PluginManager.h"
#include "Exceptions.h"

#include <iostream>

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

    if(!IsInitialized){

        _WaitForWorkerThreads();
        _CacheManager.reset();
        
        Staticinstance = nullptr;
        
        if(!SuppressSecondInstance)
            std::cout << "DualView++ Main Instance Notified. Extra instance quitting" <<
                std::endl;
        
        return;
    }

    LOG_INFO("DualView releasing resources");

    // Force close windows //
    OpenWindows.clear();

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

    _WaitForWorkerThreads();

    Staticinstance = nullptr;
}

DualView::DualView(bool tests){

    _Logger = std::make_unique<Leviathan::Logger>("test_log.txt");

    LEVIATHAN_ASSERT(tests, "DualView test constructor called with false");

    SuppressSecondInstance = true;

    Staticinstance = this;
    ThreadSpecifier = MAIN_THREAD_MAGIC;

    _CacheManager = std::make_unique<CacheManager>();

    _StartWorkerThreads();
}

DualView::DualView(std::string tests){

    _Logger = std::make_unique<Leviathan::Logger>("empty_dualview_log.txt");

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
// ------------------------------------ //
void DualView::_OnInstanceLoaded(){

    if(IsInitialized){

        {
            std::lock_guard<std::mutex> lock(QueuedCmdsMutex);
            if(QueuedCmds.empty()){

                // Pointless call to this function
                LOG_INFO("Skipping second initialization");
                return;
            }
        }
        
        _ProcessCmdQueue();
        return;
    }

    AssertIfNotMainThread();

    _Logger = std::make_unique<Leviathan::Logger>("log.txt");

    if(!_Logger){
        
        std::cerr << "Logger creation failed" << std::endl;
        abort();
        return;
    }
    
    LOG_WRITE("DualView++ Starting. Version " + std::string(DV::DUALVIEW_VERSION));

    // Create objects with simple constructors //
    _PluginManager = std::make_unique<PluginManager>();
    
    LEVIATHAN_ASSERT(_PluginManager, "Alloc failed in DualView constructor");

    // Connect dispatcher //
    StartDispatcher.connect(sigc::mem_fun(*this, &DualView::_OnLoadingFinished));
    MessageDispatcher.connect(sigc::mem_fun(*this, &DualView::_HandleMessages));
    

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

    // Start loading thread //
    LoadThread = std::thread(std::bind(&DualView::_RunInitThread, this));

    // Get rest of the widgets while load thread is already running //
    Gtk::Button* OpenImageFile = nullptr;
    MainBuilder->get_widget("OpenImageFile", OpenImageFile);
    LEVIATHAN_ASSERT(OpenImageFile, "Invalid .glade file");

    OpenImageFile->signal_clicked().connect(
        sigc::mem_fun(*this, &DualView::OpenImageFile_OnClick));
    
    
    // MainBuilder->get_widget("OpenImageFromFile", OpenImageFromFile);
    // LEVIATHAN_ASSERT(OpenImageFromFile, "Invalid .glade file");

    // Start worker threads //
    _StartWorkerThreads();

    LOG_INFO("Basic initialization completed");
    
    IsInitialized = true;
}

void DualView::_RunInitThread(){

    LOG_INFO("Running Init thread");
    LoadError = false;

    // Load settings //
    _Settings = std::make_unique<Settings>("dv_settings.levof");

    _Settings->VerifyFoldersExist();

    // Load plugins //
    //libPlugin_Imgur.so
    if(!_PluginManager->LoadPlugin("plugins/libPlugin_Imgur.so")){

        LoadError = true;
        LOG_ERROR("Failed to load plugin");
    }

    // Load ImageMagick library //
    _CacheManager = std::make_unique<CacheManager>();


    // Load database //
    

    //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
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
    
    _ProcessCmdQueue();

    OpenImageViewer("/home/hhyyrylainen/690806.jpg");
}
// ------------------------------------ //
void DualView::_ProcessCmdQueue(){

    // Skip if not properly loaded yet //
    if(!LoadCompletelyFinished){

        LOG_INFO("Skipping _ProcessCmdQueue as not completely loaded yet");
        return;
    }

    AssertIfNotMainThread();

    std::lock_guard<std::mutex> lock(QueuedCmdsMutex);

    while(!QueuedCmds.empty()){

        LOG_INFO("Running queued command");

        QueuedCmds.front()(*this);
        QueuedCmds.pop_front();
    }
}

void DualView::QueueCmd(std::function<void (DualView&)> cmd){

    std::lock_guard<std::mutex> lock(QueuedCmdsMutex);

    QueuedCmds.push_back(cmd);
}
// ------------------------------------ //
int DualView::_HandleCmdLine(const Glib::RefPtr<Gio::ApplicationCommandLine>
    &command_line)
{

    // First handle already parsed things //
    auto alreadyParsed = command_line->get_options_dict();

    Glib::ustring fileUrl;
    if(alreadyParsed->lookup_value("dl-image", fileUrl)){

        QueueCmd([fileUrl](DualView &instance) -> void
                {
                    LOG_INFO("File to download: " + std::string(fileUrl.c_str()));

                    
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

        while(!HashImageQueue.empty()){

            auto img = HashImageQueue.front().lock();

            HashImageQueue.pop_front();

            if(!img){

                // Image has been deallocated already //
                continue;
            }

            img->_DoHashCalculation();

            // Replace with an existing image if the hash exists //
            auto existing = GetImageByHash(img->GetHash());

            if(existing){

                LOG_INFO("Calculated hash for a duplicate image");
                img->BecomeDuplicateOf(*existing);
            }
        }

        HashCalculationThreadNotify.wait(lock);
    }
}
// ------------------------------------ //
void DualView::_StartWorkerThreads(){

    QuitWorkerThreads = false;

    HashCalculationThread = std::thread(std::bind(&DualView::_RunHashCalculateThread, this));
}

void DualView::_WaitForWorkerThreads(){

    // Make sure this is set //
    QuitWorkerThreads = true;

    HashCalculationThreadNotify.notify_all();

    if(HashCalculationThread.joinable())
        HashCalculationThread.join();
}

// ------------------------------------ //
bool DualView::OpenImageViewer(const std::string &file){

    AssertIfNotMainThread();

    LOG_INFO("Opening single image for viewing: " + file);

    std::shared_ptr<SingleView> window;

    try{

        window = std::make_shared<SingleView>(file);
        
    } catch(const Leviathan::InvalidArgument &e){

        LOG_WARNING("Image is not supported: " + file + " error: " + e.what());
        return false;
    }

    // Opening succeeded //
    _AddOpenWindow(window);
    return true;
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

void DualView::_AddOpenWindow(std::shared_ptr<BaseWindow> window){

    AssertIfNotMainThread();

    OpenWindows.push_back(window);
}
// ------------------------------------ //
// Database load functions
std::shared_ptr<Image> DualView::GetImageByHash(const std::string &hash){

    return nullptr;
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

