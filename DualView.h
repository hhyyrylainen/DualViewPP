#pragma once
#include <gtkmm.h>

#include <thread>
#include <atomic>
#include <memory>

namespace DV{

class PluginManager;

//! \brief Main class that contains all the windows and systems
class DualView final {
public:

    //! \brief Loads the GUI layout files and starts
    DualView(Glib::RefPtr<Gtk::Application> app);

    //! \brief Closes all resources and then attempts to close all open windows
    ~DualView();


    //! \brief Returns true if called on the main thread
    //!
    //! Used to detect errors where functions are called on the wrong thread
    static bool IsOnMainThread();
    
    //! \brief Returns the global instance or asserts and quits the program
    static DualView& Get();

protected:

    //! \brief Ran in the loader thread
    void _RunInitThread();

    //! \brief Called in the main thread once loading has completed
    //! \param succeeded True if there were no errors in the loading thread
    void _OnLoadingFinished();

private:

    // Gtk callbacks
    void OpenImageFile_OnClick();
    
private:

    Glib::RefPtr<Gtk::Application> Application;

    Glib::RefPtr<Gtk::Builder> MainBuilder;
    
    Gtk::Window* MainMenu = nullptr;
    Gtk::Window* WelcomeWindow = nullptr;

    // Startup code //
    std::thread LoadThread;
    Glib::Dispatcher StartDispatcher;

    std::atomic<bool> LoadError = { false };

    //! Plugin manager. For loading extra functionality
    std::unique_ptr<PluginManager> _PluginManager;

    static DualView* Staticinstance;
};


}

