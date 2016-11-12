// ------------------------------------ //
#include "PluginManager.h"

#include <fstream>

#ifdef __linux__
#include <dlfcn.h>
#else
#error todo: shared object opening
#endif

using namespace DV;
// ------------------------------------ //
PluginManager::PluginManager(){

}

PluginManager::~PluginManager(){

    // Unload all //
    // They are actually smart pointers so it might not unload all
    WebsiteScanners.clear();

    // Try to unload libraries //
    for(void* handle : OpenLibraryHandles){

        dlclose(handle);
    }
    
    OpenLibraryHandles.clear();
}
// ------------------------------------ //
bool PluginManager::LoadPlugin(const std::string &fileName){

    // Make sure the file exists //
    {
        std::ifstream file(fileName);

        if(!file.good()){

            LOG_ERROR("Plugin file doesn't exist: " + fileName);
            return false;
        }
    }

    LOG_INFO("Loading plugin file: " + fileName);

    void* sohandle = dlopen(fileName.c_str(), RTLD_NOW);

    if(!sohandle){

        auto* errorstr = dlerror();
            
        LOG_ERROR("Failed to load plugin library '" + fileName + "': " + (errorstr ?
                std::string(errorstr) : std::string()));

        return false;
    }

    OpenLibraryHandles.push_back(sohandle);

    CreateDescriptionFuncPtr creation;
    DestroyDescriptionFuncPtr deletion;

    // Lookup functions //

    creation = reinterpret_cast<CreateDescriptionFuncPtr>(
        dlsym(sohandle, "CreatePluginDesc"));
    deletion = reinterpret_cast<DestroyDescriptionFuncPtr>(
        dlsym(sohandle, "DestroyPluginDesc"));

    if(!creation || !deletion){

        auto* errorstr = dlerror();
        
        LOG_ERROR("Required functions not found in plugin '" + fileName + "': " + (errorstr ?
                std::string(errorstr) : std::string()));

        return false;
    }

    std::shared_ptr<IPluginDescription> plugindesc(creation() , [=](IPluginDescription* obj)
        {
            deletion(obj);
        });

    if(!plugindesc){

        LOG_ERROR("PluginDescription retrieval failed for: " + fileName);
        return false;
    }

    // Sanity check
    {
        std::string sanitystr = plugindesc->GetTheAnswer();

        if(sanitystr != std::string("42")){

            LOG_ERROR("Plugin sanity check failed for: " + fileName);
            return false;
        }
    }
    // The string destructor might also break everything so maybe install a signal handler
    // that is active while the sanity check is done
        
    // Verify version //
    std::string pluginVersion = plugindesc->GetDualViewVersionStr();

    if(pluginVersion != DUALVIEW_VERSION){

        LOG_ERROR("Plugin version mistatch: in '" + fileName + "': plugin version: " +
            pluginVersion + " required version: " + DUALVIEW_VERSION);
        return false;
    }

    // Get downloaders //
    {
        for(const auto &scanner : plugindesc->GetSupportedSites())
            AddScanner(scanner);
    }

    LOG_INFO("Plugin: " + std::string(plugindesc->GetPluginName()) + " successfully loaded (" +
        plugindesc->GetUUID() + ")");
    
    return true;
}
// ------------------------------------ //
void PluginManager::AddScanner(std::shared_ptr<IWebsiteScanner> scanner){

    for(const auto &existing : WebsiteScanners){

        if(existing->GetName() == scanner->GetName())
            return;
    }

    LOG_INFO("PluginManager: loaded new download plugin: " + std::string(scanner->GetName()));
    WebsiteScanners.push_back(scanner);
}

std::shared_ptr<IWebsiteScanner> PluginManager::GetScannerForURL(const std::string &url)
    const
{
    for(const auto& scanner : WebsiteScanners){

        if(scanner->CanHandleURL(url))
            return scanner;
    }

    return nullptr;
}
// ------------------------------------ //
void PluginManager::PrintPluginStats() const{

    LOG_INFO("PluginManager has loaded:");
    
    LOG_WRITE(Convert::ToString(WebsiteScanners.size()) +
        " website scan plugins:");

    for(const auto& dl : WebsiteScanners)
        LOG_WRITE("- " + std::string(dl->GetName()));
    LOG_WRITE("");

    LOG_WRITE("");
}
