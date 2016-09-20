// ------------------------------------ //
#include "DualView.h"
#include "Common.h"

#include "windows/SingleView.h"

#include "PluginManager.h"
#include "Exceptions.h"

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
        
    
}

DualView::~DualView(){

    LOG_INFO("DualView releasing resources");

    // Force close windows //
    OpenWindows.clear();

    // Unload plugins //
    _PluginManager.reset();

    // Close windows managed directly by us //
    WelcomeWindow->close();
    MainMenu->close();

    Staticinstance = nullptr;
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
void DualView::_RunInitThread(){

    LoadError = false;

    // Load plugins //
    //libPlugin_Imgur.so
    if(!_PluginManager->LoadPlugin("plugins/libPlugin_Imgur.so")){

        LoadError = true;
        LOG_ERROR("Failed to load plugin");
    }
    

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

