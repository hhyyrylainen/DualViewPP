//! \file Contains plugin implementation for an imgur downloader
// ------------------------------------ //
#include "core/Plugin.h"

#include <iostream>

using namespace DV;
// ------------------------------------ //
inline auto ImgurScannerID() { return "Imgur Downloader"; }
using ImgurScanner_t = cppcomponents::runtime_class<
    ImgurScannerID, cppcomponents::object_interfaces<IWebsiteScanner>>;

using ImgurScanner = cppcomponents::use_runtime_class<ImgurScanner_t>;

struct Implement_ImgurScanner : cppcomponents::implement_runtime_class<
    Implement_ImgurScanner, ImgurScanner_t>
{

    std::string GetName(){

        std::cout << "Imgur:GetName" << std::endl;
        return "Imgur Downloader";
    }

    bool CanHandleURL(std::string url){

        if(url.find("imgur.com") != std::string::npos)
            return true;

        return false;
    }
};


struct ImplementImgur_PluginDescription : cppcomponents::implement_runtime_class<
    ImplementImgur_PluginDescription, PluginDescription_t>
{
    std::vector<cppcomponents::use<IWebsiteScanner>> GetSupportedSites(){

        std::cout << "Imgur:GetSites" << std::endl;
        return {  Implement_ImgurScanner::create().QueryInterface<IWebsiteScanner>() };
    }
    
    std::vector<std::string> GetSupportedTagSites(){

        return std::vector<std::string>();
    }

    std::string GetPluginName(){

        return "Imgur Download Plugin";
    }

    std::string GetDualViewVersionStr(){

        std::cout << "Imgur:GetVersion" << std::endl;
        return DUALVIEW_VERSION;
    }
};

CPPCOMPONENTS_REGISTER(ImplementImgur_PluginDescription);
CPPCOMPONENTS_DEFINE_FACTORY();


    

