#pragma once

#include <gtkmm.h>

#include <memory>

namespace DV{

//! \brief Allows creating new Folders
class FolderCreator : public Gtk::Dialog{
public:

    //! \brief Constructor called by glade builder when loading a widget of this type
    FolderCreator(const std::string path, const std::string &prefillnewname);
    ~FolderCreator();

protected:

    Gtk::Box Container;
    
    Gtk::Entry PathEntry;
};

}
