#include "DualView.h"

#include <gtkmm.h>

int main(int argc, char* argv[]){

    auto app =
        Gtk::Application::create(argc, argv,
            "org.gtkmm.examples.base");

    app->register_application();
    
    DV::DualView dview(app);
    
    return app->run();    
}
