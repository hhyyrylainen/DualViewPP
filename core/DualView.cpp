// ------------------------------------ //
#include "DualView.h"
#include "Common.h"


#include "windows/SingleView.h"
#include "windows/CollectionView.h"
#include "windows/Importer.h"
#include "windows/TagManager.h"

#include "core/CacheManager.h"
#include "core/Database.h"
#include "core/CurlWrapper.h"

#include "resources/Collection.h"

#include "Settings.h"
#include "PluginManager.h"
#include "Exceptions.h"

#include "Common/StringOperations.h"

#include <iostream>
#include <chrono>
#include <boost/filesystem.hpp>

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

void DualView::OpenImageViewer(std::shared_ptr<Image> image){

    AssertIfNotMainThread();
    
    auto window = std::make_shared<SingleView>(image);
        
    // Opening succeeded //
    _AddOpenWindow(window);
}

void DualView::OpenImporter(){

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
    _AddOpenWindow(wrapped);
    wrapped->show();
}

void DualView::OpenTagCreator(const std::string &settext){

    OpenTagCreator();

    _TagManager->SetCreateTag(settext);
}

void DualView::OpenTagCreator(){

    // Show it //
    Application->add_window(*_TagManager);
    _TagManager->show();
    _TagManager->present();
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
    if (collectionname.empty())
    {
        addtocollection = uncategorized;
        canapplytags = false;

    } else
    {
        addtocollection = GetOrCreateCollection(collectionname, IsInPrivateMode);

        if (!addtocollection)
            throw Leviathan::InvalidArgument("Invalid collection name");
    }

    LEVIATHAN_ASSERT(addtocollection, "Failed to get collection object");
    
    if (canapplytags)
        addtocollection->AddTags(addcollectiontags);

    size_t currentitem = 0;
    const auto maxitems = resources.size();

    auto order = addtocollection->GetLastShowOrder();

    std::vector<std::string> filestodelete;

    // Save resources to database if not loaded from the database //

    for(auto& resource : resources)
    {
        std::shared_ptr<Image> actualresource;

        if (!resource->IsInDatabase())
        {
            // If the image hash is in the collection then we shouldn't be here //

            if (!MoveFileToCollectionFolder(resource, addtocollection, move))
            {
                LOG_ERROR("Failed to move file to collection's folder");
                return false;
            }
            
            std::shared_ptr<TagCollection> tagstoapply;

            // Store tags for applying //
            if (resource->GetTags()->HasTags())
                tagstoapply = resource->GetTags();

            try
            {
                _Database->InsertImage(*resource);
            }
            catch (const InvalidSQL &e)
            {
                // We have already moved the image so this is a problem
                LOG_INFO("TODO: move file back after adding fails");
                LOG_ERROR("Sql error adding image to collection: ");
                e.PrintToLog();
                return false;
            }

            // Apply tags //
            if (tagstoapply)
                resource->GetTags()->AddTags(*tagstoapply);

            actualresource = resource;
        } else
        {
            actualresource = resource;

            // Remove from uncategorized if not adding to that //
            if(addtocollection != uncategorized)
            {
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

    do
    {
        exists = false;

        for(const std::string& file : filestodelete)
        {
            if (boost::filesystem::exists(file))
            {
                exists = true;

                try{
                    boost::filesystem::remove(file);
                } catch(const boost::filesystem::filesystem_error&){

                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

    } while (exists);

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

    UncategorizedCollection = _Database->SelectCollectionByNameAG("Uncategorized");
    return UncategorizedCollection;
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

