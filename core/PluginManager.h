#pragma once

#include "Plugin.h"

#include <vector>
#include <string>

//! \file Implements a plugin manager that can be used to load plugins. And later query
//! if the plugins offer some new feature

namespace DV{

class DualView;

//! \brief Plugin manager class
//!
//! Loads plugins from dynamic libraries
class PluginManager{
    friend DualView;
public:
    
    PluginManager();

    //! \brief Ensures plugins are closed properly
    ~PluginManager();

    //! \brief Prints stats about loaded plugins
    void PrintPluginStats() const;

    //! \brief Returns the best scanner for url, or null
    std::shared_ptr<IWebsiteScanner> GetScannerForURL(const std::string &url) const;

protected:

    void AddScanner(std::shared_ptr<IWebsiteScanner> scanner);

    //! \brief Tries to load plugin from a file
    //! \returns True on success
    bool LoadPlugin(const std::string &fileName);

private:

    //! Open .so handles
    std::vector<void*> OpenLibraryHandles;
    
    //! Loaded Website scanners
    std::vector<std::shared_ptr<IWebsiteScanner>> WebsiteScanners;
};
}

