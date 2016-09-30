#pragma once


#include "leviathan/ObjectFiles/ObjectFile.h"


namespace DV{

//! \brief Contains runtime settings
class Settings{
public:

    //! \brief Tries to load settings from file
    //!
    //! If the file doesn't exist it will be created on save with default settings
    Settings(const std::string &file);

    ~Settings();

    //! \brief Verifies that the settings folders exists, if they don't creates them
    void VerifyFoldersExist() const;

    //! \brief Forces a save of the settings
    void Save();


    

protected:

    //! \brief Called when a setting has changed
    void _MarkDirty();

protected:

    // Main settings //
    //! The folder where the sqlite database is loaded from
    std::string DatabaseFolder = "./";

    //! The base folder for public collection
    
    
    

    //! Settings storage object
    //! \todo This will be used to preserve comments in the file, currently does nothing
    
};

}

