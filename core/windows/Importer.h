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

    //! File drag received
    void _OnFileDropped(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y,
        const Gtk::SelectionData& selection_data, guint info, guint time);

    bool _OnDragMotion(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y,
        guint time);

    bool _OnDrop(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time);
    

protected:

    SuperViewer* PreviewImage;
    SuperContainer* ImageList;

    bool DoingImport = false;
};
      

}
