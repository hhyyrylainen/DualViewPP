#pragma once

#include "BaseWindow.h"

#include "core/components/FolderSelector.h"

#include <gtkmm.h>

namespace DV{

class Collection;
class FolderSelector;

class AddToFolder : public BaseWindow, public Gtk::Window{
public:

    AddToFolder(std::shared_ptr<Collection> collection);
    ~AddToFolder();

protected:

    void _OnClose() override;

    //! \brief Applies the effect and then closes
    void _OnApply();

protected:

    FolderSelector TargetFolder;
    std::shared_ptr<Collection> MovedCollection;

    Gtk::Box MainBox;
    Gtk::Box ButtonBox;

    Gtk::Button Accept;
    Gtk::Button Cancel;
};


}
