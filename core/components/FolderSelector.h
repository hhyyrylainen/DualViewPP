#pragma once

#include "SuperContainer.h"
#include "FolderNavigatorHelper.h"

#include <gtkmm.h>

#include <memory>

namespace DV{

class Folder;

//! \brief Allows selecting a DV::Folder
class FolderSelector : public Gtk::Box, public FolderNavigatorHelper{
public:

    //! \brief Non-glade constructor
    FolderSelector();
    
    //! \brief Constructor called by glade builder when loading a widget of this type
    FolderSelector(_GtkBox* widget, Glib::RefPtr<Gtk::Builder> builder);
    ~FolderSelector();

    //! \brief Returns the current folder. This is never null
    std::shared_ptr<Folder> GetFolder() const{

        return CurrentFolder;
    }

private:

    void _CommonCtor();
    
protected:

    //! \brief Reads the properties of the SelectedFolder
    void OnFolderChanged() override;

    // Gtk callbacks //
    void _CreateNewFolder();

protected:
    


    //! Shows the CurrentPath to the user and can be used to paste in a path
    Gtk::Entry PathEntry;

    Gtk::Button CreateNewFolder;
    Gtk::Button UpFolder;

    Gtk::Box TopBox;

    //! Contents of the current folder
    SuperContainer FolderContents;
};

}
