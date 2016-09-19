#include "DualView.h"

#include <gtkmm.h>

#include "Common.h"

int main(int argc, char* argv[]){

    Logger log("log.txt");

    LOG_WRITE("DualView++ Starting. Version " + std::string(DUALVIEW_VERSION));
    
    auto app =
        Gtk::Application::create(argc, argv,
            "org.gtkmm.examples.base");
    
    app->register_application();
    
    DV::DualView dview(app);

    LOG_INFO("Init completed. Running");
    
    const auto ret = app->run();

    LOG_INFO("Event Loop Has Exited. Quitting");

    return ret;
}
