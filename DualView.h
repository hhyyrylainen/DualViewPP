#pragma once
#include <gtkmm.h>

#include "windows/BaseWindow.h"

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

    //! \brief Opens an image viewer for a file
    //! \returns True if opened. False if the file isn't supported
    bool OpenImageViewer(const std::string &file);

    //! \brief Registers a gtk window with the gtk instance
    void RegisterWindow(Gtk::Window &window);

    //! \brief Adds a closed message to the queue and invokes the main thread
    //! \param event The event that has a pointer to the closed window. This
    //! won't be dereferenced.
    //! \note This usually gets called twice when closing windows
    void WindowClosed(std::shared_ptr<WindowClosedEvent> event);
    
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

    //! \brief Called when messages are received to handle them
    void _HandleMessages();

    //! \brief Adds a new window to the open list
    //!
    //! This is needed to make sure that they aren't deallocated immediately
    void _AddOpenWindow(std::shared_ptr<BaseWindow> window);
    
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

    //! Used to call the main thread when a message has been added
    Glib::Dispatcher MessageDispatcher;

    //! Must be locked when any of the message queues is changed
    std::mutex MessageQueueMutex;

    //! When windows have closed or they want to be closed they send
    //! an event here through DualView::WindowClosed
    //! MessageQueueMutex must be locked when changing this
    std::list<std::shared_ptr<WindowClosedEvent>> CloseEvents;

    //! List of open windows
    //! Used to keep the windows allocated while they are open
    std::vector<std::shared_ptr<BaseWindow>> OpenWindows;

    //! Plugin manager. For loading extra functionality
    std::unique_ptr<PluginManager> _PluginManager;

    static DualView* Staticinstance;
};


}

