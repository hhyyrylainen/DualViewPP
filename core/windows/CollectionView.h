#pragma once

#include "BaseWindow.h"

#include "core/components/ImageListItem.h"

#include <gtkmm.h>

namespace DV{

//! \brief Window that shows all the (image) things in the database
class CollectionView : public Gtk::Window{
public:

    CollectionView(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~CollectionView();

private:

    bool _OnClose(GdkEventAny* event);
    
private:

    std::vector<std::shared_ptr<ListItem>> stuffs;
};

}

