#pragma once

#include "BaseWindow.h"

#include <gtkmm.h>

namespace DV{

//! \brief Window that shows all the tags and allows editing them
class TagManager : public Gtk::Window{
public:

    TagManager(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~TagManager();

    //! \brief Fills in the name field of a new tag
    void SetCreateTag(const std::string);

private:

    bool _OnClose(GdkEventAny* event);

    void _OnShown();
    void _OnHidden();
    
private:

};

}

