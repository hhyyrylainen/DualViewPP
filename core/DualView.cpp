// ------------------------------------ //
#include "DualView.h"
#include "Common.h"


#include "windows/SingleView.h"
#include "windows/CollectionView.h"
#include "windows/Importer.h"
#include "windows/TagManager.h"
#include "windows/FolderCreator.h"
#include "windows/SingleCollection.h"

#include "core/CacheManager.h"
#include "core/Database.h"
#include "core/CurlWrapper.h"

#include "resources/Collection.h"
#include "resources/Image.h"
#include "resources/Tags.h"
#include "resources/Folder.h"

#include "Settings.h"
#include "PluginManager.h"
#include "Exceptions.h"

#include "Common/StringOperations.h"
#include "Iterators/StringIterator.h"

#include <iostream>
#include <chrono>
#include <boost/filesystem.hpp>
#include <boost/locale.hpp>

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
        _Settings.reset();
        
        Staticinstance = nullptr;
        
        if(!SuppressSecondInstance)
            std::cout << "DualView++ Main Instance Notified. Extra instance quitting" <<
                std::endl;
        
        return;
    }

    LOG_INFO("DualView releasing resources");

    // Force close windows //
    OpenWindows.clear();

    _CollectionView.reset();
    _TagManager.reset();

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

    if(!dbfile.empty())
        _Database = std::make_unique<Database>(dbfile);

    _StartWorkerThreads();
}

DualView::DualView(std::string tests, std::unique_ptr<Database> &&db /*= nullptr*/){

    _Logger = std::make_unique<Leviathan::Logger>("empty_dualview_log.txt");

    if(db)
        _Database = std::move(db);

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

    Gtk::Button* OpenImporter = nullptr;
    MainBuilder->get_widget("OpenImporter", OpenImporter);
    LEVIATHAN_ASSERT(OpenImporter, "Invalid .glade file");

    OpenImporter->signal_clicked().connect(
        sigc::mem_fun(*this, &DualView::OpenImporter));

    Gtk::Button* OpenTags = nullptr;
    MainBuilder->get_widget("OpenTags", OpenTags);
    LEVIATHAN_ASSERT(OpenTags, "Invalid .glade file");

    OpenTags->signal_clicked().connect(
        sigc::mem_fun<void>(*this, &DualView::OpenTagCreator));
    

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
    //libPlugin_Imgur.so
    if(!_PluginManager->LoadPlugin("plugins/libPlugin_Imgur.so")){

        LOG_ERROR("Failed to load plugin");
        return true;
    }

    // Load ImageMagick library //
    _CacheManager = std::make_unique<CacheManager>();

    // Load database //
    //_Database = std::make_unique<Database>(true);
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
    
    _ProcessCmdQueue();

    // For testing SuperViewer uncomment this to quickly open images
    //OpenImageViewer("/home/hhyyrylainen/690806.jpg");
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

        HashCalculationThreadNotify.wait(lock);
    }
}
// ------------------------------------ //
void DualView::QueueDBThreadFunction(std::function<void()> func){

    std::lock_guard<std::mutex> lock(DatabaseFuncQueueMutex);

    DatabaseFuncQueue.push_back(std::make_unique<std::function<void()>>(func));

    DatabaseThreadNotify.notify_all();
}

void DualView::_RunDatabaseThread(){

    std::unique_lock<std::mutex> lock(HashImageQueueMutex);

    while(!QuitWorkerThreads){

        while(!DatabaseFuncQueue.empty()){

            auto func = std::move(DatabaseFuncQueue.front());

            DatabaseFuncQueue.pop_front();

            lock.unlock();

            func-> operator()();
            
            lock.lock();
        }

        DatabaseThreadNotify.wait(lock);
    }
}
// ------------------------------------ //
void DualView::InvokeFunction(std::function<void()> func){

    std::unique_lock<std::mutex> lock(InvokeQueueMutex);

    InvokeQueue.push_back(func);

    // Notify main thread
    InvokeDispatcher.emit();
}

void DualView::_ProcessInvokeQueue(){

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
}

void DualView::_WaitForWorkerThreads(){

    // Make sure this is set //
    QuitWorkerThreads = true;

    HashCalculationThreadNotify.notify_all();
    DatabaseThreadNotify.notify_all();

    if(HashCalculationThread.joinable())
        HashCalculationThread.join();

    if(DatabaseThread.joinable())
        DatabaseThread.join();

    if(DateInitThread.joinable())
        DateInitThread.join();
}
// ------------------------------------ //
std::string DualView::GetPathToCollection(bool isprivate) const{

    if(isprivate){

        return _Settings->GetPrivateCollection();
        
    } else {

        return _Settings->GetPublicCollection();
    }
}

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

            boost::filesystem::rename(img->GetResourcePath(), finalPath);
        
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
// ------------------------------------ //
bool DualView::IsFileContent(const std::string &file){

    const auto extension = boost::filesystem::path(file).extension();
    
    for(const auto& type : SUPPORTED_EXTENSIONS){

        if(std::get<0>(type) == extension)
            return true;
    }

    return false;
}

std::string DualView::StringToLower(const std::string &str){

    boost::locale::generator gen;
    return boost::locale::to_lower(str, gen(""));
}
// ------------------------------------ //
bool DualView::OpenImageViewer(const std::string &file){

    AssertIfNotMainThread();

    LOG_INFO("Opening single image for viewing: " + file);

    OpenImageViewer(Image::Create(file));
    return true;
}

void DualView::OpenImageViewer(std::shared_ptr<Image> image){

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
    
    wrapped->Open(image);
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

void DualView::OpenTagCreator(const std::string &settext){

    OpenTagCreator();

    _TagManager->SetCreateTag(settext);
}

void DualView::OpenTagCreator(){

    AssertIfNotMainThread();

    // Show it //
    Application->add_window(*_TagManager);
    _TagManager->show();
    _TagManager->present();
}

void DualView::RunFolderCreatorAsDialog(const std::string path,
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

    std::string name, createpath;
    window.GetNewName(name, createpath);

    LOG_INFO("Trying to create new folder \"" + name + "\" at: " + createpath);

    try{
        auto parent = GetFolderFromPath(createpath);

        if(!parent)
            throw std::runtime_error("Parent folder at path doesn't exist");

        auto created = _Database->InsertFolder(name, isprivate, *parent);

        if(!created)
            throw std::runtime_error("Failed to create folder");
        
    } catch(const std::exception &e){

        auto dialog = Gtk::MessageDialog(parentwindow,
            "Failed to create folder \"" + name + "\" at: " + createpath,
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
            
        } else {
            
            actualresource = resource;

            // Remove from uncategorized if not adding to that //
            if(addtocollection != uncategorized){
                
                uncategorized->RemoveImage(actualresource);
            }
        }

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
std::shared_ptr<Folder> DualView::GetFolderFromPath(const std::string &path){

    // Root folder //
    if(path == "Root" || path == "Root/"){

        return GetRootFolder();
    }

    // Loop through all the path components and verify that a folder exists
    // This is actually overkill for what we are doing
    Leviathan::StringIterator itr(path);

    std::shared_ptr<Folder> currentfolder;

    while(!itr.IsOutOfBounds()){

        auto part = itr.GetUntilNextCharacterOrAll<std::string>('/');

        if(!part){

            // String ended //
            return currentfolder;
        }

        if(!currentfolder && (*part == "Root")){

            currentfolder = GetRootFolder();
            continue;
        }

        if(!currentfolder){

            // Didn't begin with root //
            return nullptr;
        }

        // Find a folder with the current name inside currentfolder //
        auto nextfolder = _Database->SelectFolderByNameAndParentAG(*part, *currentfolder);

        if(!nextfolder){

            // There's a nonexistant folder in the path
            return nullptr;
        }

        // Moved to the next part
        currentfolder = nextfolder;
    }
    
    return currentfolder;
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
std::vector<std::string> DualView::GetSuggestionsForTag(std::string str) const{

    // Strip whitespace //
    Leviathan::StringOperations::RemovePreceedingTrailingSpaces(str);

    if(str.empty())
        return {};

    // Convert to lower //
    str = StringToLower(str);

    std::vector<std::string> result;
    
    // We want to provide suggestions for modifiers, tags and combines
    Leviathan::StringIterator itr(str);

    std::string currentpart;
    std::unique_ptr<std::string> currentword;

    // Used to prepend the valid part to suggestions
    std::string prefix;

    while(!itr.IsOutOfBounds()){

        currentword = itr.GetUntilNextCharacterOrAll<std::string>(' ');

        if(!currentword)
            continue;

        currentpart += *currentword;

        // If it is a valid tag component clear it //
        if(IsStrValidTagPart(currentpart)){

            prefix += currentpart + " ";
            currentpart.clear();
            continue;
        }
    }
    
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
        if(currentword)
            RetrieveTagsMatching(result, *currentword);
        
    } else {

        // Get suggestions for it //
        std::vector<std::string> tmpholder;
        RetrieveTagsMatching(tmpholder, currentpart);

        result.reserve(result.size() + tmpholder.size());

        for(const auto& gotmatch : tmpholder){

            result.push_back(prefix + gotmatch);
        }
    }

    // Sort the most relevant results first //
    std::sort(result.begin(), result.end(), [&str](
            const std::string &left, const std::string &right) -> bool
        {
            if(left == str && (right != str)){

                // Exact match first //
                return true;
            }
            
            if(Leviathan::StringOperations::StringStartsWith(left, str) && 
                !Leviathan::StringOperations::StringStartsWith(right, str))
            {
                // Matching prefix with pattern first
                return true;
            }

            // Sort which one is closer in length to str first
            return std::abs(str.length() - left.length()) <
                std::abs(str.length() - right.length());
        });

    return result;
}

bool DualView::IsStrValidTagPart(const std::string &str) const{

    auto byname = _Database->SelectTagByNameOrAlias(str);
    if(byname)
        return true;

    auto mod = _Database->SelectTagModifierByNameOrAliasAG(str);
    if(mod)
        return true;

    auto rule = _Database->SelectTagBreakRuleByStr(str);

    if(rule){

        std::string tagname;
        std::shared_ptr<Tag> tag;
        auto modifiers = rule->DoBreak(str, tagname, tag);

        if(tag || !modifiers.empty()){

            // Rule matched
            return true;
        }
    }

    auto alias = _Database->SelectTagSuperAlias(str);
    if(!alias.empty())
        return true;

    // Combined with works if we can remove the first word and the second part is valid //
    auto withoutfirst = Leviathan::StringOperations::RemoveFirstWords(str, 1);

    if(!withoutfirst.empty() && withoutfirst != str){

        return IsStrValidTagPart(withoutfirst);
    }

    return false;
}

void DualView::RetrieveTagsMatching(std::vector<std::string> &result,
    const std::string &str) const
{
    const auto initSize = result.size();
    
    _Database->SelectTagNamesWildcard(result, str);
    
    _Database->SelectTagAliasesWildcard(result, str);

    _Database->SelectTagModifierNamesWildcard(result, str);
    
    _Database->SelectTagBreakRulesByStrWildcard(result, str);

    _Database->SelectTagSuperAliasWildcard(result, str);

    // Combined with works if we can remove the first word
    // But only if we didn't already get good matches
    if(result.size() - initSize < 2){
        
        auto withoutfirst = Leviathan::StringOperations::RemoveFirstWords(str, 1);

        if(!withoutfirst.empty() && withoutfirst != str){

            RetrieveTagsMatching(result, withoutfirst);
        }
    }
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

