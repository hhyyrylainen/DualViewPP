// ------------------------------------ //
#include "DualView.h"
#include "Common.h"

using namespace DV;
// ------------------------------------ //
DualView::DualView(Glib::RefPtr<Gtk::Application> app) : Application(app){

    Staticinstance = this;

    // Connect dispatcher //
    StartDispatcher.connect(sigc::mem_fun(*this, &DualView::_OnLoadingFinished));

    MainBuilder = Gtk::Builder::create_from_file(
        "../gui/main_gui.glade");

    // Get all the glade resources //
    MainBuilder->get_widget("WelcomeWindow", WelcomeWindow);
    LEVIATHAN_ASSERT(WelcomeWindow, "Invalid .glade file");
    
    MainBuilder->get_widget("MainMenu", MainMenu);
    LEVIATHAN_ASSERT(MainMenu, "Invalid .glade file");

    // Show the loading window //
    Application->add_window(*WelcomeWindow);
    WelcomeWindow->show();

    // Start loading thread //


    LoadThread = std::thread(std::bind(&DualView::_RunInitThread, this));
}

DualView::~DualView(){

    WelcomeWindow->close();
    MainMenu->close();

    Staticinstance = nullptr;
}

DualView* DualView::Staticinstance = nullptr;

DualView& DualView::Get(){

    LEVIATHAN_ASSERT(Staticinstance, "DualView static instance is null");
    return *Staticinstance;
}
// ------------------------------------ //
void DualView::_RunInitThread(){

    LoadError = false;

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    // Invoke the callback on the main thread //
    StartDispatcher.emit();
}

void DualView::_OnLoadingFinished(){

    // The thread needs to be joined or an exception is thrown
    if(LoadThread.joinable())
        LoadThread.join();

    if(LoadError){

        // Loading failed
        LOG_ERROR("Loading Failed");
        WelcomeWindow->close();
        return;
    }

    LOG_INFO("Loading Succeeded");

    Application->add_window(*MainMenu);
    MainMenu->show();

    // Hide the loading window after, just in case
    WelcomeWindow->close();
}
// ------------------------------------ //


