#pragma once

#include "core/IsAlive.h"

#include <gtkmm.h>

#include <atomic>
#include <thread>
#include <condition_variable>
#include <future>

namespace DV{

class DLListItem;
class NetGallery;

struct DownloadProgressState;

//! \brief Window that has all the download objects
//! and also implements the download algorithm
class Downloader : public Gtk::Window, public IsAlive{
    
    friend DownloadProgressState;
    
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

    //! \brief Gets the next selected download gallery
    std::shared_ptr<DLListItem> GetNextSelectedGallery();

    void _RunDownloadThread();

    void _DLFinished(std::shared_ptr<DLListItem> item);
    
    void _SetDLThreadStatus(const std::string &statusstr, bool spinneractive, float progress);

    void _OnRemoveListItem(DLListItem &item);

protected:

    //! \warning If this is a flowbox all hell breaks loose
    Gtk::Box* DLWidgets;


    Gtk::Button* StartDownloadButton;
    Gtk::Label* DLStatusLabel;
    Gtk::Spinner* DLSpinner;
    Gtk::LevelBar* DLProgress;

    // Download thread //
    std::atomic<bool> RunDownloadThread = { false };

    std::thread DownloadThread;
    std::condition_variable NotifyDownloadThread;
    std::mutex DownloadThreadMutex;
    
    //! All currently not finished downloads
    std::vector<std::shared_ptr<DLListItem>> DLList;
};
}
