#pragma once

#include "BaseWindow.h"

#include <gtkmm.h>

namespace DV{

class SuperContainer;

//! \brief Window that shows all the (image) things in the database
class CollectionView : public Gtk::Window{
public:

    CollectionView(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~CollectionView();

private:

    bool _OnClose(GdkEventAny* event);

    void _OnShown();
    void _OnHidden();
    
private:

    SuperContainer* Container = nullptr;
};

}

