// ------------------------------------ //
#include "Settings.h"

#include "DualView.h"
#include "Common.h"

#include "leviathan/ObjectFiles/ObjectFileProcessor.h"

#include <boost/filesystem.hpp>

using namespace DV;
// ------------------------------------ //
Settings::Settings(const std::string &file) : SettingsFile(file){

    LEVIATHAN_ASSERT(!SettingsFile.empty(), "Settings file empty");

    if(boost::filesystem::exists(SettingsFile))
        _Load();
}

Settings::~Settings(){

    if(IsDirty)
        Save();
}
// ------------------------------------ //
void Settings::Save(){

    using namespace Leviathan;

    IsDirty = false;

    LOG_INFO("Saving settings to " + SettingsFile);

    // Create the file structure //
    ObjectFile data;

    data.AddNamedVariable(std::make_shared<NamedVariableList>("SettingsVersion",
            new IntBlock(SETTINGS_VERSION)));

    data.AddNamedVariable(std::make_shared<NamedVariableList>("SavedWithVersion",
            new StringBlock(std::string("DualView ") + DUALVIEW_VERSION)));

    // Collection / database settings //
    {
        auto collection = std::make_shared<ObjectFileObjectProper>("Collection",
            "", std::vector<std::unique_ptr<std::string>>());

        auto collectionList = std::make_unique<ObjectFileListProper>("settings");

        collectionList->AddVariable(std::make_shared<NamedVariableList>("DatabaseFolder",
                new StringBlock(DatabaseFolder)));

        collectionList->AddVariable(std::make_shared<NamedVariableList>("PublicCollection",
                new StringBlock(PublicCollection)));

        collectionList->AddVariable(std::make_shared<NamedVariableList>("PrivateCollection",
                new StringBlock(PrivateCollection)));
    
        collection->AddVariableList(std::move(collectionList));

        data.AddObject(collection);
    }

    // Image view settings //
    {
        auto images = std::make_shared<ObjectFileObjectProper>("Images",
            "", std::vector<std::unique_ptr<std::string>>());

        auto imagesDelays = std::make_unique<ObjectFileListProper>("delays");

        imagesDelays->AddVariable(std::make_shared<NamedVariableList>("NextImage",
                new FloatBlock(NextImageDelay)));
    
        images->AddVariableList(std::move(imagesDelays));

        auto imagesPreload = std::make_unique<ObjectFileListProper>("pre-load");

        imagesPreload->AddVariable(std::make_shared<NamedVariableList>("CollectionForward",
                new IntBlock(PreloadCollectionForward)));

        imagesPreload->AddVariable(std::make_shared<NamedVariableList>("CollectionBackwards",
                new IntBlock(PreloadCollectionBackwards)));
    
        images->AddVariableList(std::move(imagesPreload));

        data.AddObject(images);
    }

    if(!ObjectFileProcessor::WriteObjectFile(data, SettingsFile,
            DualView::Get().GetLogger()))
    {
        LOG_ERROR("Saving settings failed");
    }
}

void Settings::_Load(){

    LOG_INFO("Loading settings from: " + SettingsFile);

    auto file = Leviathan::ObjectFileProcessor::ProcessObjectFile(SettingsFile,
        DualView::Get().GetLogger());

    if(!file){
        
        LOG_ERROR("Failed to parse configuration file");
        throw Leviathan::InvalidArgument("Settings file is malformed");
    }

    auto log = DualView::Get().GetLogger();

    int settingsVersion;
    Leviathan::ObjectFileProcessor::LoadValueFromNamedVars(file->GetVariables(),
        "SettingsVersion", settingsVersion, -1, log, "Settings: Load:");

    if(!IsVersionCompatible(settingsVersion)){

        LOG_ERROR("Loaded version " + Convert::ToString(settingsVersion) +
            " is incompatible with the current version: " +
            Convert::ToString(SETTINGS_VERSION));
        
        throw Leviathan::InvalidState("Settings file version is incompatible");
    }

    std::shared_ptr<Leviathan::ObjectFileObject> collection = nullptr;
    std::shared_ptr<Leviathan::ObjectFileObject> images = nullptr;

    for(size_t i = 0; i < file->GetTotalObjectCount(); ++i){

        const auto& obj = file->GetObject(i);

        if(obj->GetName() == "Collection"){

            collection = obj;
            
        } else if(obj->GetName() == "Images"){

            images = obj;
        }
    }

    // Collection settings //
    if(collection){

        auto settings = collection->GetListWithName("settings");

        if(settings){

            Leviathan::ObjectFileProcessor::LoadValueFromNamedVars(settings->GetVariables(),
                "DatabaseFolder", DatabaseFolder, DatabaseFolder,
                log, "Settings: Load:");

            Leviathan::ObjectFileProcessor::LoadValueFromNamedVars(settings->GetVariables(),
                "PublicCollection", PublicCollection, PublicCollection,
                log, "Settings: Load:");

            Leviathan::ObjectFileProcessor::LoadValueFromNamedVars(settings->GetVariables(),
                "PrivateCollection", PrivateCollection, PrivateCollection,
                log, "Settings: Load:");
            
        } else {

            LOG_WARNING("Settings Collection missing settings list");
        }
        
    } else {

        LOG_WARNING("Settings file missing Collection settings");
    }

    // Image settings //
    if(images){

        auto delays = images->GetListWithName("delays");

        if(delays){

            Leviathan::ObjectFileProcessor::LoadValueFromNamedVars(delays->GetVariables(),
                "NextImage", NextImageDelay, NextImageDelay,
                log, "Settings: Load:");
            
        } else {

            LOG_WARNING("Settings Images missing delays list");
        }

        auto preload = images->GetListWithName("pre-load");

        if(preload){

            Leviathan::ObjectFileProcessor::LoadValueFromNamedVars(preload->GetVariables(),
                "CollectionForward", PreloadCollectionForward, PreloadCollectionForward,
                log, "Settings: Load:");

            Leviathan::ObjectFileProcessor::LoadValueFromNamedVars(preload->GetVariables(),
                "CollectionBackwards", PreloadCollectionBackwards, PreloadCollectionBackwards,
                log, "Settings: Load:");
            
        } else {

            LOG_WARNING("Settings Images missing preload list");
        }
        
    } else {

        LOG_WARNING("Settings file missing Images settings");
    }
    
}

bool Settings::IsVersionCompatible(int loadversion){

    if(loadversion == SETTINGS_VERSION){

        return true;
    }

    // Invalid version //
    if(loadversion <= 0)
        return false;

    // Future version //
    if(loadversion > SETTINGS_VERSION){

        return false;
    }

    // Should be fine //
    return true;
}
// ------------------------------------ //

void Settings::_MarkDirty(){

    IsDirty = true;
}
// ------------------------------------ //
inline void CreateFolder(const boost::filesystem::path &p){

    if(!is_directory(p)){

        LOG_INFO("Creating folder: " + std::string(p.c_str()));
        boost::filesystem::create_directories(p);
    }
}

void Settings::VerifyFoldersExist() const{

    using namespace boost::filesystem;

    CreateFolder(path(DatabaseFolder));

    // Public
    CreateFolder(path(PublicCollection));

    CreateFolder(path(PublicCollection) / path("collections/"));
    CreateFolder(path(PublicCollection) / path("no_category/"));
    CreateFolder(path(PublicCollection) / path("_trash/"));

    // Private
    CreateFolder(path(PrivateCollection));

    CreateFolder(path(PrivateCollection) / path("collections/"));
    CreateFolder(path(PrivateCollection) / path("no_category/"));
    CreateFolder(path(PrivateCollection) / path("_trash/"));

    // Special folders
    CreateFolder(path(PrivateCollection) / path("staging/"));
    CreateFolder(path(PrivateCollection) / path("thumbnails/"));
}
// ------------------------------------ //

