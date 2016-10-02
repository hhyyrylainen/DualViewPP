#pragma once

#include "ListItem.h"

#include <gtkmm.h>

namespace DV{

//! \brief Holds ListItem derived widgets and arranges them in a scrollable box
class SuperContainer : public Gtk::ScrolledWindow{
public:
    
    //! \brief Constructor called by glade builder when loading a widget of this type
    SuperContainer(_GtkScrolledWindow* widget, Glib::RefPtr<Gtk::Builder> builder);
    ~SuperContainer();
    
protected:
    
    
private:
    
    Gtk::Viewport View;
    Gtk::Fixed Container;

    std::vector<std::shared_ptr<ListItem>> stuffs;
};

}
