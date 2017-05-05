#pragma once

#include "BaseWindow.h"
#include "core/IsAlive.h"

#include <gtkmm.h>

namespace DV{

//! \brief Window that has all sorts of buttons for debugging
class DebugWindow : public Gtk::Window, public IsAlive{
public:

    DebugWindow(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~DebugWindow();

    //! Creates a task that keeps DB thread busy, allowing testing
    //! things that hang the main thread when DB is being used
    void OnMakeDBBusy();

    //! Opens an Magick::Image to make sure there isn't memory leakage
    void OnTestImageRead();

    //! Tests that objects don't leave traces. Needs to be ran with a leak detector
    void OnTestInstanceCreation();
    
private:

    bool _OnClose(GdkEventAny* event);

    void _OnHidden();

private:

};

}

