#pragma once

#include <gtkmm.h>
#include <string>
#include <memory>

namespace DV{

class Folder;

//! \brief Contains common functions for navigating between folders
class FolderNavigatorHelper{
public:

    FolderNavigatorHelper() = default;

    //! \brief Goes back to root folder
    void GoToRoot();

    //! \brief Goes to the specified path, or to Root if the path is invalid
    void GoToPath(const std::string &path);

    //! \brief Tries to go to the specified path, if invalid does nothing
    //! \returns True on success
    bool TryGoToPath(const std::string &path);

    //! \brief Goes to a subfolder
    void MoveToSubfolder(const std::string &subfoldername);
    
protected:

    virtual void OnFolderChanged() = 0;

    // Gtk callbacks //
    void _OnUpFolder();

    //! \todo Play error sound on fail and don't go to root
    void _OnPathEntered();

    //! \brief Registers default events. After calling this GoToRoot should be called
    void RegisterNavigator(Gtk::Entry &pathentry, Gtk::Button &upfolder);
    
protected:

    Gtk::Entry* NavigatorPathEntry = nullptr;

    std::shared_ptr<Folder> CurrentFolder;

    //! Because a folder can have multiple paths we keep track of the current one
    std::string CurrentPath;
};

}
