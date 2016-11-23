#pragma once

#include "leviathan/ObjectFiles/ObjectFile.h"

#include <boost/filesystem.hpp>

namespace DV{

//! The current version of the configuration file format
constexpr auto SETTINGS_VERSION = 1;

//! \brief Contains runtime settings
//! \todo Make sure that folders are recreated if settings are changed
class Settings{
public:

    //! \brief Tries to load settings from file
    //!
    //! If the file doesn't exist it will be created on save with default settings
    Settings(const std::string &file);

    ~Settings();

    //! \brief Verifies that the settings folders exists, if they don't creates them
    //! \exception Leaks boost exceptions
    void VerifyFoldersExist() const;

    //! \brief Forces a save of the settings
    void Save();

    //! \brief Returns PrivateCollection
    const std::string& GetPrivateCollection() const{
        
        return PrivateCollection;
    }

    //! \brief Returns PublicCollection
    const std::string& GetPublicCollection() const{
        
        return PublicCollection;
    }

    //! \brief Returns a path to the staging folder
    const std::string GetStagingFolder() const{

        return (boost::filesystem::path(PrivateCollection) / "staging/").string();
    }


    //! \brief Returns the database file
    const auto GetDatabaseFile() const{

        return std::string((boost::filesystem::path(DatabaseFolder) /
                boost::filesystem::path("dualview.sqlite")).c_str());
    }

    //! \brief Sets the private collection
    void SetPrivateCollection(const std::string &newfolder, bool save = true){

        PrivateCollection = newfolder;
        IsDirty = true;

        if(save)
            Save();
    }

    //! \brief Returns executable relative path to Folder icon
    std::string GetPathToFolderIcon() const{

        return "../gui/icons/file-folder.png";
    }

    //! \brief Returns executable relative path to Collection icon
    std::string GetPathToCollectionIcon() const{

        return "../gui/icons/folders.png";
    }

    auto GetCurlDebug() const{

        return CurlDebug;
    }

    auto GetPluginList() const{

        return PluginsToLoad;
    }

    auto GetPluginFolder() const{

        return PluginFolder;
    }

    //! \brief Returns true if loadversion is compatible with SETTINGS_VERSION
    static bool IsVersionCompatible(int loadversion);

protected:

    //! \brief Called when a setting has changed
    void _MarkDirty();

    //! \brief Loads settings from a file
    //! \note The file must exist for this to do anything
    void _Load();

protected:

    //! \brief If true saving is disabled
    //!
    //! Used for running tests
    bool InMemoryOnly = false;

    //! If true needs to be saved to disk
    bool IsDirty = true;

    //! The file this will be saved to
    std::string SettingsFile;

    // Main settings //
    //! The folder where the sqlite database is loaded from
    std::string DatabaseFolder = "./";

    //! The base folder for public collection
    std::string PublicCollection = "./public_collection/";

    //! The base folder for private collection
    std::string PrivateCollection = "./private_collection/";
    

    // Image view settings //
    //! When next image key is held down how long is each image shown
    float NextImageDelay = 0.2f;

    //! How many images ahead of the current one are loaded
    int32_t PreloadCollectionForward = 3;

    //! How many images behind the current one are loaded
    int32_t PreloadCollectionBackwards = 1;

    //! True if curl should print debug output
    bool CurlDebug = false;

    //! List of plugins that need to be loaded
    std::vector<std::string> PluginsToLoad = { "Plugin_Imgur" };

    //! Folder where plugin files are stored in
    std::string PluginFolder = "plugins/";

    //! Settings storage object
    //! \todo This will be used to preserve comments in the file, currently does nothing
    
};

}

