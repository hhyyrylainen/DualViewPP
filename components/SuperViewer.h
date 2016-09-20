#pragma once

#include "core/resources/Image.h"

#include <gtkmm.h>

namespace DV{

class SuperViewer : public Gtk::DrawingArea{
public:

    
    //! \brief Constructor called by glade builder when loading a widget of this type
    SuperViewer(_GtkDrawingArea* area, Glib::RefPtr<Gtk::Builder> builder,
        std::shared_ptr<Image> displayedResource);
    
    
protected:

    
};
}
