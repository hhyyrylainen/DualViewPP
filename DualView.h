#pragma once
#include <gtkmm.h>
#include <thread>

#include <atomic>

namespace DV{

//! \brief Main class that contains all the windows and systems
class DualView final {
public:

    //! \brief Loads the GUI layout files and starts
    DualView(Glib::RefPtr<Gtk::Application> app);

    //! \brief Closes all resources and then attempts to close all open windows
    ~DualView();

    static DualView& Get();

protected:

    //! \brief Ran in the loader thread
    void _RunInitThread();

    //! \brief Called in the main thread once loading has completed
    //! \param succeeded True if there were no errors in the loading thread
    void _OnLoadingFinished();

private:

    Glib::RefPtr<Gtk::Application> Application;

    Glib::RefPtr<Gtk::Builder> MainBuilder;
    
    Gtk::Window* MainMenu = nullptr;
    Gtk::Window* WelcomeWindow = nullptr;

    // Startup code //
    std::thread LoadThread;
    Glib::Dispatcher StartDispatcher;

    std::atomic<bool> LoadError = { false };

    static DualView* Staticinstance;
};


}

