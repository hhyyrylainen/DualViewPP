#pragma once

#include "Common.h"

//! \file Defines the interface for DualView plugins to implement
#include "cppcomponents/cppcomponents.hpp"

#include <vector>
#include <string>

namespace DV{

//! \brief Description of a plugin
//!
//! When loading plugins this is the first thing that is loaded from the plugin and
//! based on the definitions the plugin is added to the right places to be used
//! later
struct IPluginDescription : cppcomponents::define_interface<
    cppcomponents::uuid<0xd9c0d740, 0x7e96, 0x11e6, 0x9820, 0x305a3a06584e>>
{

    //! \returns A list of regexes for sites that this plugin supports
    std::vector<std::string> GetSupportedSites();
    
    //! \returns A list of regexes for tag download sites that this plugin supports
    std::vector<std::string> GetSupportedTagSites();

    //! \returns The name of the plugin
    std::string GetPluginName();

    //! \returns The constant DUALVIEW_VERSION_STR
    std::string GetDualViewVersionStr();

    CPPCOMPONENTS_CONSTRUCT(IPluginDescription, GetSupportedSites, GetSupportedTagSites,
        GetPluginName, GetDualViewVersionStr);
};

inline auto PluginDescriptionID() { return "PluginDescription"; }

// using PluginDescription_t = cppcomponents::runtime_class<
//     PluginDescriptionID, cppcomponents::factory_interface<
//                              cppcomponents::NoConstructorFactoryInterface>,
//     cppcomponents::static_interfaces<IPluginDescription>>;
using PluginDescription_t = cppcomponents::runtime_class<
    PluginDescriptionID, cppcomponents::object_interfaces<IPluginDescription>>;


using PluginDescription = cppcomponents::use_runtime_class<PluginDescription_t>;

// This will probably be used if cppcomponents::NoConstructorFactoryInterface doesn't
// work
//cppcomponents::object_interfaces<IPluginDescription>


}


