#pragma once

#include "core/IsAlive.h"

#include <gtkmm.h>

namespace DV{

class DLListItem;

//! \brief Window that has all the download objects
//! and also implements the download algorithm
class Downloader : public Gtk::Window, public IsAlive{
public:

    Downloader(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~Downloader();

protected:
    
    bool _OnClose(GdkEventAny* event);

    void _OnShown();
    void _OnHidden();

protected:

    Gtk::FlowBox* DLWidgets;
    
    //! All currently not finished downloads
    std::vector<std::shared_ptr<DLListItem>> DLList;
};
}
