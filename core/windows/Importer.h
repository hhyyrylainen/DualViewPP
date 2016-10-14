#pragma once

#include "BaseWindow.h"

#include <gtkmm.h>


namespace DV{

class SuperContainer;
class SuperViewer;

class Importer : public BaseWindow, public Gtk::Window{
public:

    Importer(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~Importer();
    
    
protected:
    
    bool _OnClosed(GdkEventAny* event);

    void _OnClose() override;

protected:

    SuperViewer* PreviewImage;
    SuperContainer* ImageList;
};
      

}
