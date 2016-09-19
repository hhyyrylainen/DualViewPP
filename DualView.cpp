// ------------------------------------ //
#include "DualView.h"

using namespace DV;
// ------------------------------------ //

DualView::DualView(Glib::RefPtr<Gtk::Application> app) : Application(app){

    MainBuilder = Gtk::Builder::create_from_file(
        "../gui/main_gui.glade");

    
    MainBuilder->get_widget("WelcomeWindow", WelcomeWindow);
    
    Application->add_window(*WelcomeWindow);
    //WelcomeWindow->show();


    MainBuilder->get_widget("MainMenu", MainMenu);

    Application->add_window(*MainMenu);
    MainMenu->show();
}


