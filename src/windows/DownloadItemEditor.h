#pragma once

#include "BaseWindow.h"
#include "IsAlive.h"

#include "Plugin.h"
#include "components/PrimaryMenu.h"

#include <gtkmm.h>

#include <atomic>

namespace DV {

class NetGallery;
class NetFile;
class PageScanJob;
class InternetImage;

//! \brief Allows editing the download options of a gallery
class DownloadItemEditor : public BaseWindow, public Gtk::Window, public IsAlive {

    struct ScanJobData;
public:
    DownloadItemEditor(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder,
        const std::shared_ptr<NetGallery>& toedit);
    ~DownloadItemEditor();

    void LoadDownloadProperties();

    void StartReferrerScan();

    void StopReferrerScan();


protected:
    void _OnClose() override;

    void OnStartStopReferrerScanPressed();
    //! \todo Make this a reversible action
    void OnAcceptNewLinks();

    void _UpdateReferrerWidgets();
    void _OnReferrerScanCompleted(const ScanResult& result);
private:
    static void QueueNextThing(std::shared_ptr<ScanJobData> data, DownloadItemEditor* editor,
        IsAlive::AliveMarkerT alive, std::shared_ptr<PageScanJob> scanned);
private:
    const std::shared_ptr<NetGallery> EditedItem;
    std::vector<std::shared_ptr<NetFile>> CurrentFilesForItem;

    std::vector<std::shared_ptr<InternetImage>> FoundRefreshedItems;

    std::atomic<bool> ScanningForFreshLinks{false};

    Gtk::HeaderBar* HeaderBar;
    Gtk::MenuButton* Menu;

    // Primary menu
    PrimaryMenu MenuPopover;

    // Fix links page
    Gtk::Button* ScanReferrersForLinks;
    Gtk::Label* ReferrerScanStatus;
    Gtk::ProgressBar* ReferrerScanProgress;
    Gtk::Button* ReferrerScanAcceptResult;
};

} // namespace DV
