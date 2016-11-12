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
    cppcomponents::use<IWebsiteScanner> GetScannerForURL(const std::string &url) const;

protected:

    void AddScanner(const cppcomponents::use<IWebsiteScanner> scanner);

    //! \brief Tries to load plugin from a file
    //! \returns True on success
    bool LoadPlugin(const std::string &fileName);

private:

    //! Loaded Website scanners
    std::vector<cppcomponents::use<IWebsiteScanner>> WebsiteScanners;
};
}

