#pragma once

#include "BaseWindow.h"

#include "core/components/FolderSelector.h"
#include "core/IsAlive.h"

#include <gtkmm.h>

namespace DV{

class Collection;

class RemoveFromFolders : public BaseWindow, public Gtk::Window, public IsAlive{
public:

    RemoveFromFolders(std::shared_ptr<Collection> collection);
    ~RemoveFromFolders();

    //! \brief Reads the folders collection is in
    void ReadFolders();

protected:

    void _OnClose() override;

protected:

    std::shared_ptr<Collection> TargetCollection;

    Gtk::Box MainBox;
    
    Gtk::Button ApplyButton;
};


}
