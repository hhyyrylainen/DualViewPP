//! \file Contains plugin implementation for an imgur downloader
// ------------------------------------ //
#include "core/Plugin.h"

using namespace DV;
// ------------------------------------ //

struct ImplementImgur_PluginDescription : cppcomponents::implement_runtime_class<
    ImplementImgur_PluginDescription , PluginDescription_t>
{
    std::vector<std::string> GetSupportedSites(){

        return { std::string("imgur\\.com") };
    }
    
    std::vector<std::string> GetSupportedTagSites(){

        return std::vector<std::string>();
    }

    std::string GetPluginName(){

        return "Imgur Download Plugin";
    }

    std::string GetDualViewVersionStr(){

        return DUALVIEW_VERSION;
    }
};

CPPCOMPONENTS_REGISTER(ImplementImgur_PluginDescription);
CPPCOMPONENTS_DEFINE_FACTORY();
    

