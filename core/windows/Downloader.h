#pragma once

#include "core/IsAlive.h"

#include <gtkmm.h>

#include <atomic>
#include <thread>
#include <condition_variable>

namespace DV{

class DLListItem;
class NetGallery;

//! \brief Window that has all the download objects
//! and also implements the download algorithm
class Downloader : public Gtk::Window, public IsAlive{
public:

    Downloader(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~Downloader();

    //! \brief Adds a NetGallery to be shown
    void AddNetGallery(std::shared_ptr<NetGallery> gallery);

    //! \brief Spawns the downloader thread
    void StartDownloadThread();

    //! \brief Signals download to stop at the next convenient time
    void StopDownloadThread();

    //! \brief Waits until download thread has quit
    void WaitForDownloadThread();

protected:

    //! \brief Toggles the download thread, callback for the button
    void _ToggleDownloadThread();
    
    bool _OnClose(GdkEventAny* event);

    void _OnShown();
    void _OnHidden();

    void _OpenNewDownloadSetup();


    void _RunDownloadThread();

protected:

    Gtk::FlowBox* DLWidgets;


    Gtk::Button* StartDownloadButton;
    Gtk::Label* DLStatusLabel;
    Gtk::Spinner* DLSpinner;

    // Download thread //
    std::atomic<bool> RunDownloadThread;

    std::thread DownloadThread;
    std::condition_variable NotifyDownloadThread;
    std::mutex DownloadThreadMutex;
    
    //! All currently not finished downloads
    std::vector<std::shared_ptr<DLListItem>> DLList;
};
}
