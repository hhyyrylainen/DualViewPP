#pragma once

#include "BaseWindow.h"
#include "core/components/FolderNavigatorHelper.h"
#include "core/IsAlive.h"

#include <gtkmm.h>

namespace DV{

class SuperContainer;
class ResourceWithPreview;
class Folder;

//! \brief Window that shows all the (image) things in the database
//! \todo Create a base class for all the path moving functions and callbacks
class CollectionView : public Gtk::Window, public IsAlive, public FolderNavigatorHelper{
public:

    CollectionView(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~CollectionView();

    void GoToPath(const std::string &path);

private:

    bool _OnClose(GdkEventAny* event);

    void _OnShown();
    void _OnHidden();

    void OnFolderChanged() override;
    
private:

    SuperContainer* Container = nullptr;

    Gtk::Entry* PathEntry;
    Gtk::Button* UpFolder;
};

}

