#pragma once

#include "Common.h"

#include <memory>

#include <vector>
#include <string>

namespace DV{

//! \brief Implementation of a website scanner
class IWebsiteScanner{
public:
    
    //! \brief Returns a user readable name string, may also contain version
    virtual const char* GetName() = 0;

    //! \brief Returns true if this plugin can handle url
    virtual bool CanHandleURL(const std::string &url) = 0;
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


