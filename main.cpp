#include "DualView.h"

#include <gtkmm.h>

int main(int argc, char* argv[]){

    auto app =
        Gtk::Application::create(argc, argv,
            "org.gtkmm.examples.base");
    
    Glib::RefPtr<Gtk::Builder> builder = Gtk::Builder::create_from_file(
        "../gui/main_gui.glade");

    app->register_application();
    
    builder->get_widget("WelcomeWindow", WelcomeWindow);
    
    app->add_window(*WelcomeWindow);
    WelcomeWindow->show();


    DV::DualView dview(app);
    


    builder->get_widget("MainMenu", mainWindow);

    app->add_window(*mainWindow);
    mainWindow->show();

    return app->run();    
}
