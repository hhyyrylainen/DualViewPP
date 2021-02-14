#pragma once

#include "BaseWindow.h"

#include "components/FolderSelector.h"

#include <gtkmm.h>

namespace DV {

class Collection;
class FolderSelector;

//! \brief GUI for adding things to folders
class AddToFolder : public BaseWindow, public Gtk::Window {
private:
    AddToFolder();
public:
    AddToFolder(std::shared_ptr<Collection> collection);
    AddToFolder(std::shared_ptr<Folder> folder);
    ~AddToFolder();

protected:
    void _OnClose() override;

    //! \brief Applies the effect and then closes
    void _OnApply();

    //! \brief Updates the shown text based on which MovedCollection, AddedFolder is not null
    void _UpdateLabelsForType();

protected:
    FolderSelector TargetFolder;
    std::shared_ptr<Collection> MovedCollection;
    std::shared_ptr<Folder> AddedFolder;

    Gtk::Box MainBox;
    Gtk::Box ButtonBox;

    Gtk::Button Accept;
    Gtk::Button Cancel;
};


} // namespace DV
