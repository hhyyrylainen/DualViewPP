#pragma once

#include <gtkmm.h>

namespace DV{

//! \brief Main class that contains all the windows and systems
class DualView final {
public:

    //! \brief Loads the GUI layout files and starts
    DualView(Glib::RefPtr<Gtk::Application> app);
    
    

private:

    Glib::RefPtr<Gtk::Application> Application;

    Glib::RefPtr<Gtk::Builder> MainBuilder;
    
    Gtk::Window* MainMenu = nullptr;
    Gtk::Window* WelcomeWindow = nullptr;
    
    

    
    static DualView* Staticinstance;
};


}

