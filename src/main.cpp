#include "DualView.h"

#include <gtkmm.h>

#include <iostream>

int main(int argc, char* argv[])
{
    auto app = Gtk::Application::create(
        argc, argv, "com.boostslair.dualview", Gio::APPLICATION_HANDLES_COMMAND_LINE);

    // clang-format off
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
    app->add_main_option_entry(
        Gio::Application::OPTION_TYPE_STRING,
        "dl-page",
        '\0',
        "Open downloader with the page open",
        "http://file.url.com/"
    );
    app->add_main_option_entry(
        Gio::Application::OPTION_TYPE_STRING,
        "dl-auto",
        '\0',
        "Open downloader with the link with automatic detection",
        "http://file.url.com/img.png"
    );
    app->add_main_option_entry(
        Gio::Application::OPTION_TYPE_STRING,
        "dl-referrer",
        '\0',
        "An URL that is the referrer for another type of dl-option that was passed in",
        "http://file.url.com/"
    );
    // clang-format on

    if(!app->register_application()) {

        std::cerr << "Register failed" << std::endl;
        std::cout << "Registering application failed. Quitting" << std::endl;
        return 1;
    }

    DV::DualView dview(app);

    return app->run();
}
