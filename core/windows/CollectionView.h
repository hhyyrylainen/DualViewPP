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
class CollectionView : public Gtk::Window, public FolderNavigatorHelper,
                         public std::enable_shared_from_this<CollectionView>
{
public:

    CollectionView(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~CollectionView();

private:

    bool _OnClose(GdkEventAny* event);

    void _OnShown();
    void _OnHidden();

    void OnFolderChanged() override;

    void OnSearchChanged();

    //! \brief Common contents fill for OnFolderChanged and OnSearchChanged
    void _UpdateShownItems();
    
private:

    SuperContainer* Container = nullptr;

    Gtk::Entry* PathEntry;
    Gtk::Button* UpFolder;

    Gtk::SearchEntry* SearchBox;
};

}

