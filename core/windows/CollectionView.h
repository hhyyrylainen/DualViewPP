#pragma once

#include "BaseWindow.h"
#include "core/IsAlive.h"

#include "Common/ThreadSafe.h"

#include <gtkmm.h>

namespace DV{

class SuperContainer;
class ResourceWithPreview;
class Folder;

//! \brief Window that shows all the (image) things in the database
class CollectionView : public Gtk::Window, public IsAlive, public Leviathan::ThreadSafe{
public:

    CollectionView(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~CollectionView();

private:

    bool _OnClose(GdkEventAny* event);

    void _OnShown();
    void _OnHidden();
    
private:

    SuperContainer* Container = nullptr;

    //! The current folder from which items are shown
    std::shared_ptr<Folder> CurrentFolder;
};

}

