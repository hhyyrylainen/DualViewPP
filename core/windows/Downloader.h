#pragma once

#include "core/IsAlive.h"

#include <gtkmm.h>

namespace DV{

class DLListItem;
class NetGallery;

//! \brief Window that has all the download objects
//! and also implements the download algorithm
class Downloader : public Gtk::Window, public IsAlive{
public:

    Downloader(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~Downloader();

    //! \brief Adds a NetGallery to be shown
    void AddNetGallery(std::shared_ptr<NetGallery> gallery);

protected:
    
    bool _OnClose(GdkEventAny* event);

    void _OnShown();
    void _OnHidden();

    void _OpenNewDownloadSetup();

protected:

    Gtk::FlowBox* DLWidgets;
    
    //! All currently not finished downloads
    std::vector<std::shared_ptr<DLListItem>> DLList;
};
}
