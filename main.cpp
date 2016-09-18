#include <gtkmm.h>

int main(int argc, char* argv[]){

    auto app =
        Gtk::Application::create(argc, argv,
            "org.gtkmm.examples.base");

    Glib::RefPtr<Gtk::Builder> builder = Gtk::Builder::create_from_file(
        "../gui/main_gui.glade");

    Gtk::Window* mainWindow = nullptr;
    builder->get_widget("MainMenu", mainWindow);


    return app->run(*mainWindow);    
}
