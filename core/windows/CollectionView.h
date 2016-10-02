#pragma once

#include "BaseWindow.h"

#include "core/components/ImageListItem.h"
#include "core/components/SuperContainer.h"

#include <gtkmm.h>

namespace DV{

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

