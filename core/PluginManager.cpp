// ------------------------------------ //
#include "PluginManager.h"

#include <fstream>

using namespace DV;
// ------------------------------------ //
PluginManager::PluginManager(){

}

PluginManager::~PluginManager(){

    // Unload all //
    // They are actually smart pointers so it might not unload all
    WebsiteScanners.clear();
    
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

    try{
        LOG_INFO("Loading plugin file: " + fileName);

        auto p = PluginDescription::dynamic_creator(
            fileName, "PluginDescription")();
        
        //auto version = PluginDescription::static_interface(fileName,
        //PluginDescriptionID()).GetDualViewVersionStr();

        if(!p){

            LOG_ERROR("PluginDescription retrieval failed for: " + fileName);
            return false;
        }

        // Verify version //
        std::string pluginVersion = p.GetDualViewVersionStr();

        if(pluginVersion != DUALVIEW_VERSION){

            LOG_ERROR("Plugin version mistatch: in '" + fileName + "': plugin version: " +
                pluginVersion + " required version: " + DUALVIEW_VERSION);
            return false;
        }

        // Get downloaders //
        {
            for(const auto &scanner : p.GetSupportedSites())
                AddScanner(scanner);
        }

        LOG_INFO("Plugin: " + p.GetPluginName() + " successfully loaded");
        return true;
    
    } catch (const cppcomponents::error_unable_to_load_library &e){

        LOG_ERROR("Failed to load plugin library '" + fileName + "': " + e.what());
        return false;
    }


}
// ------------------------------------ //
void PluginManager::AddScanner(const cppcomponents::use<IWebsiteScanner> scanner){

    for(const auto &existing : WebsiteScanners){

        if(existing.GetName() == scanner.GetName())
            return;
    }

    LOG_INFO("PluginManager: loaded new download plugin: " + scanner.GetName());
    WebsiteScanners.push_back(scanner);
}

cppcomponents::use<IWebsiteScanner> PluginManager::GetScannerForURL(const std::string &url)
    const
{
    for(const auto& scanner : WebsiteScanners){

        if(scanner.CanHandleURL(url))
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
        LOG_WRITE("- " + dl.GetName());
    LOG_WRITE("");

    LOG_WRITE("");
}
