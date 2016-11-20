#pragma once

#include "Common.h"

#include <memory>

#include <vector>
#include <string>

namespace DV{

//! \brief Scan result that has a content link
struct ScanFoundImage{

    ScanFoundImage(const std::string &url, const std::string &referrer) :
        URL(url), Referrer(referrer)
    {
        
    }
    
    bool operator ==(const ScanFoundImage &other) const{

        return URL == other.URL;
    }

    //! Merges tags from other
    void Merge(const ScanFoundImage &other){

    }

    std::string URL;
    std::string Referrer;
    //std::vector<std::string> Tags
};

//! \brief Result data for IWebsiteScanner
struct ScanResult{
public:
    
    //! \brief Used by scanners to add a link with no tags
    void AddContentLink(const ScanFoundImage &link){

        for(auto& existinglink : ContentLinks){

            if(existinglink == link){
                existinglink.Merge(link);
                return;
            }
        }
        
        ContentLinks.push_back(link);
    }

    //! \brief Used by scanners when more pages for a gallery are found
    void AddSubpage(const std::string &url){

        for(auto& existinglink : PageLinks){

            if(existinglink == url)
                return;
        }
        
        PageLinks.push_back(url);
    }

    //! \brief Used by scanners to add tags to currently scanned thing
    void AddTagStr(const std::string &tag){

        for(auto& existingtag : PageTags){

            if(existingtag == tag)
                return;
        }
        
        PageTags.push_back(tag);
    }

    void PrintInfo() const{

        LOG_INFO("ScanResult: has " + Convert::ToString(ContentLinks.size()) +
            " found images and " + Convert::ToString(PageLinks.size()) + " page links and " +
            Convert::ToString(PageTags.size()) + " page tags");
    }

    void Combine(const ScanResult &other){

        for(const auto &inother : other.ContentLinks)
            AddContentLink(inother);

        for(const auto &inother : other.PageLinks)
            AddSubpage(inother);

        for(const auto &inother : other.PageTags)
            AddTagStr(inother);

        if(!other.PageTitle.empty())
            PageTitle += "; " + other.PageTitle;
    }

    std::vector<ScanFoundImage> ContentLinks;
    std::vector<std::string> PageLinks;
    std::vector<std::string> PageTags;

    //! Title of the scanned page
    //! \note Scan plugins should remove unneeded parts from this. For example if the
    //! title has the site name that should be removed
    std::string PageTitle;
};

//! \brief Implementation of a website scanner
class IWebsiteScanner{
public:
    
    //! \brief Returns a user readable name string, may also contain version
    virtual const char* GetName() = 0;

    //! \brief Returns true if this plugin can handle url
    virtual bool CanHandleURL(const std::string &url) = 0;

    //! \brief Returns true if this plugin uses URL rewriting
    virtual bool UsesURLRewrite() = 0;

    //! \brief Returns URL after rewriting.
    //!
    //! Only valid if UsesURLRewrite returns true
    virtual std::string RewriteURL(const std::string &url) = 0;

    //! \brief Scans a webpage
    //! \param contenttype Content type sent by the server. Probably equals "test/html"
    virtual ScanResult ScanSite(const std::string &body, const std::string &url,
        const std::string &contenttype) = 0;
};

//! \brief Description of a plugin
//!
//! When loading plugins this is the first thing that is loaded from the plugin and
//! based on the definitions the plugin is added to the right places to be used
//! later
class IPluginDescription{
public:
    
    //! \returns A unique string from uuid for this plugin
    virtual const char* GetUUID() = 0;

    //! \returns The (user readable) name of the plugin
    virtual const char* GetPluginName() = 0;
    
    //! \returns The constant DUALVIEW_VERSION
    virtual const char* GetDualViewVersionStr() = 0;

    //! \returns A list of Website scanners
    virtual std::vector<std::shared_ptr<IWebsiteScanner>> GetSupportedSites() = 0;

    //! \brief For checking that everything is fine
    virtual std::string GetTheAnswer() = 0;
};

// These function pointers must match the functions that the plugins expose
// Also they must be called CreatePluginDesc DestroyPluginDesc and defined
// with c linkage
using CreateDescriptionFuncPtr = IPluginDescription* (*)();
using DestroyDescriptionFuncPtr =  void (*)(IPluginDescription*);

}


