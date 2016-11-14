//! \file Contains plugin implementation for an imgur downloader
// ------------------------------------ //
#include "core/Plugin.h"

#include "Common.h"

#include "leviathan/Common/StringOperations.h"

#include "GQ/src/Document.hpp"

#include <regex>

using namespace DV;
// ------------------------------------ //

static const auto IUContentLink = std::regex(".*i.imgur.com\\/.+",
    std::regex::ECMAScript | std::regex::icase);

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

        auto testDocument = gq::Document::Create();
        testDocument->Parse(body);

        try
        {
            testDocument->Each(".post-images .post-image", [&](const gq::Node* node){

                    // Child link of class 'zoom' should have href as a link to content
                    // The above statement is only true if javascript is enabled,
                    // which currently isn't

                    node->Each(".post-images .post-image", [&](const gq::Node* contentLink){

                            if(!contentLink->HasAttribute(boost::string_ref("href")))
                                return;

                            auto link = contentLink->GetAttributeValue(
                                boost::string_ref("href"));

                            if(std::regex_match(std::begin(link), std::end(link),
                                    IUContentLink))
                            {
                                // Combine the url to make it absolute //
                                result.AddContentLink(
                                    Leviathan::StringOperations::CombineURL(url,
                                        link.to_string()));
                            }
                        });
                    
                    // Directly images //
                    auto images = node->Find("img");

                    if(images.GetNodeCount() > 0 && images.GetNodeAt(0)->HasAttribute(
                            boost::string_ref("src")))
                    {
                        auto link = images.GetNodeAt(0)->GetAttributeValue(
                            boost::string_ref("src"));
                        
                        if(std::regex_match(std::begin(link), std::end(link),
                                IUContentLink))
                        {
                            // Combine the url to make it absolute //
                            result.AddContentLink(
                                Leviathan::StringOperations::CombineURL(url,
                                    link.to_string()));
                        }
                    }


                    // Unloaded images //
                    node->Each(".post-image-container", [&](const gq::Node* contentLink){

                            const auto linkid = contentLink->GetAttributeValue(
                                boost::string_ref("id"));
                            
                            // For gifs this is required //
                            // TODO: webms
                            auto videos = contentLink->Find(".video-container");
                            
                            for(size_t i = 0; i < videos.GetNodeCount(); ++i){

                                // Needs to download as gif
                                result.AddContentLink(
                                    Leviathan::StringOperations::URLProtocol(url) +
                                    "://i.imgur.com/" + linkid.to_string() + ".gif");
                            
                            }

                            result.AddSubpage(Leviathan::StringOperations::CombineURL(url,
                                    "/" + linkid.to_string()));
                        });
                });
        }
        catch(std::runtime_error& e)
        {
            // Errors from the css selection
            LOG_ERROR("GQ selector error: " + std::string(e.what()));
            return result;
        }

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


    

