#pragma once

#include "SuperContainer.h"

#include <gtkmm.h>

#include <memory>

namespace DV{

class Folder;

//! \brief Allows selecting a DV::Folder
class FolderSelector : public Gtk::Box{
public:

    //! \brief Non-glade constructor
    FolderSelector();
    
    //! \brief Constructor called by glade builder when loading a widget of this type
    FolderSelector(_GtkBox* widget, Glib::RefPtr<Gtk::Builder> builder);
    ~FolderSelector();

    //! \brief Returns the current folder. This is never null
    std::shared_ptr<Folder> GetFolder() const{

        return SelectedFolder;
    }

    //! \brief Goes back to root folder
    void GoToRoot();

    //! \brief Goes to the specified path, or to Root if the path is invalid
    void GoToPath(const std::string &path);

private:

    void _CommonCtor();
    
protected:

    //! \brief Reads the properties of the SelectedFolder
    void OnFolderChanged();

    // Gtk callbacks //
    void _OnUpFolder();
    void _CreateNewFolder();

protected:
    
    std::shared_ptr<Folder> SelectedFolder;

    //! Because a folder can have multiple paths we keep track of the current one
    std::string CurrentPath;

    //! Shows the CurrentPath to the user and can be used to paste in a path
    Gtk::Entry PathEntry;

    Gtk::Button CreateNewFolder;
    Gtk::Button UpFolder;

    Gtk::Box TopBox;

    //! Contents of the current folder
    SuperContainer FolderContents;
};

}
