//! \file Contains plugin implementation for an imgur downloader
// ------------------------------------ //
#include "core/Plugin.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //

class ImgurScanner final : public IWebsiteScanner{


    const char* GetName() override{

        return "Imgur Downloader"; 
    }

    bool CanHandleURL(const std::string &url) override{
        
        if(url.find("imgur.com") != std::string::npos)
            return true;

        return false;
    }

    ScanResult ScanSite(const std::string &body, const std::string &url) override{

        ScanResult result;

        LOG_INFO("ImgurScanner: scanning page: " + url);

        

        return result;
    }
};


class ImgurPluginDescription final : public IPluginDescription{
    
    const char* GetUUID() override{

        return "b1ed014c-a90e-11e6-92f7-305a3a06584e";
    }

    //! \returns The (user readable) name of the plugin
    const char* GetPluginName() override{

        return "Imgur Download Plugin"; 
    }
    
    const char* GetDualViewVersionStr() override{

        return DUALVIEW_VERSION;
    }

    std::vector<std::shared_ptr<IWebsiteScanner>> GetSupportedSites() override{

        return {
            std::shared_ptr<IWebsiteScanner>(new ImgurScanner(), [=](IWebsiteScanner* obj)
                {
                    auto* casted = static_cast<ImgurScanner*>(obj);
                    delete casted;
                })
                };
    }

    std::string GetTheAnswer() override{

        return std::string("42");
    }
};

extern "C"{

    IPluginDescription* CreatePluginDesc(){

        return new ImgurPluginDescription;
    }

    void DestroyPluginDesc(IPluginDescription* desc){

        if(!desc)
            return;

        auto* casted = static_cast<ImgurPluginDescription*>(desc);
        delete casted;
    }
}


    

