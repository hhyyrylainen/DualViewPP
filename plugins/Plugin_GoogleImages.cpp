//! \file Contains plugin implementation for a Google Images downloader
// ------------------------------------ //
#include "core/Plugin.h"

#include <iostream>

using namespace DV;
// ------------------------------------ //
inline auto GoogleImagesScannerID() { return "Google Images Downloader"; }
using GoogleImagesScanner_t = cppcomponents::runtime_class<
    GoogleImagesScannerID, cppcomponents::object_interfaces<IWebsiteScanner>>;

using GoogleImagesScanner = cppcomponents::use_runtime_class<GoogleImagesScanner_t>;

struct Implement_GoogleImagesScanner : cppcomponents::implement_runtime_class<
    Implement_GoogleImagesScanner, GoogleImagesScanner_t>
{

    std::string GetName(){
        
        std::cout << "Google:GetName" << std::endl;
        return "Google Images Downloader";
    }

    bool CanHandleURL(std::string url){

        if(url.find(".google.") != std::string::npos)
            return true;

        return false;
    }
};

struct ImplementGoogleImages_PluginDescription : cppcomponents::implement_runtime_class<
    ImplementGoogleImages_PluginDescription , PluginDescription_t>
{
    std::vector<cppcomponents::use<IWebsiteScanner>> GetSupportedSites(){

        std::cout << "Google:GetSites" << std::endl;
        return {  Implement_GoogleImagesScanner::create().QueryInterface<IWebsiteScanner>() };
    }
    
    std::vector<std::string> GetSupportedTagSites(){

        return std::vector<std::string>();
    }

    std::string GetPluginName(){

        return "Google Images Download Plugin";
    }

    std::string GetDualViewVersionStr(){

        std::cout << "Google:GetVersion" << std::endl;
        return DUALVIEW_VERSION;
    }
};

CPPCOMPONENTS_REGISTER(ImplementGoogleImages_PluginDescription);
CPPCOMPONENTS_DEFINE_FACTORY();


    

