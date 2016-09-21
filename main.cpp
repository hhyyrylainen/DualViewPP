#include "DualView.h"

#include <gtkmm.h>

#include <iostream>

int main(int argc, char* argv[]){

    auto app =
        Gtk::Application::create(argc, argv,
            "com.boostslair.dualview", Gio::APPLICATION_HANDLES_COMMAND_LINE);

    // Add command line argument handling //
    app->add_main_option_entry(
        Gio::Application::OPTION_TYPE_BOOL,
        "version",
        'v',
        "Print version number",
        ""
    );
    app->add_main_option_entry(
        Gio::Application::OPTION_TYPE_STRING,
        "dl-image",
        '\0',
        "Open downloader with the image open",
        "http://file.url.com/img.png"
    );
    
    if(!app->register_application()){

        std::cerr << "Register failed" << std::endl;
        return 1;
    }
    
    DV::DualView dview(app);

    return app->run();
}
