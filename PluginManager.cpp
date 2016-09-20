// ------------------------------------ //
#include "PluginManager.h"

#include <fstream>

using namespace DV;
// ------------------------------------ //
PluginManager::~PluginManager(){

    
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
        auto p = PluginDescription::dynamic_creator(
            fileName, "PluginDescription")();

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

        LOG_INFO("Plugin: " + p.GetPluginName() + " successfully loaded");
        return true;
    
    } catch (const cppcomponents::error_unable_to_load_library &e){

        LOG_ERROR("Failed to load plugin library '" + fileName + "': " + e.what());
        return false;
    }


}
