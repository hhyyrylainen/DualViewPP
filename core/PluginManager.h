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
    PluginManager() = default;

    //! \brief Ensures plugins are closed properly
    ~PluginManager();

protected:

    //! \brief Tries to load plugin from a file
    //! \returns True on success
    bool LoadPlugin(const std::string &fileName);

private:

    
};
}

