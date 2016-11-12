//! \file Contains plugin implementation for a Google Images downloader
// ------------------------------------ //
#include "core/Plugin.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //

class GoogleImagesScanner final : public IWebsiteScanner{


    const char* GetName() override{

        return "Google Images Downloader"; 
    }

    bool CanHandleURL(const std::string &url) override{
        
        if(url.find(".google.") != std::string::npos)
            return true;

        return false;
    }
};


class GoogleImagesPluginDescription final : public IPluginDescription{
    
    const char* GetUUID() override{

        return "6209f09e-a90f-11e6-9d8e-305a3a06584e";
    }

    //! \returns The (user readable) name of the plugin
    const char* GetPluginName() override{

        return "GoogleImages Download Plugin"; 
    }
    
    const char* GetDualViewVersionStr() override{

        return DUALVIEW_VERSION;
    }

    std::vector<std::shared_ptr<IWebsiteScanner>> GetSupportedSites() override{

        return {
            std::shared_ptr<IWebsiteScanner>(new GoogleImagesScanner(),
                [=](IWebsiteScanner* obj)
                {
                    auto* casted = static_cast<GoogleImagesScanner*>(obj);
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

        return new GoogleImagesPluginDescription;
    }

    void DestroyPluginDesc(IPluginDescription* desc){

        if(!desc)
            return;

        auto* casted = static_cast<GoogleImagesPluginDescription*>(desc);
        delete casted;
    }
}
    

