#pragma once

#include "BaseWindow.h"
#include "IsAlive.h"

#include "components/PrimaryMenu.h"

#include <gtkmm.h>

#include <atomic>
#include <thread>

namespace DV {

//! \brief Tool for deleting images from a path that are already imported, helper to quickly
//! check if a lot of images are already imported or not
class AlreadyImportedImageDeleter : public Gtk::Window, public IsAlive {
public:
    AlreadyImportedImageDeleter(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~AlreadyImportedImageDeleter();

    void OnStartStopPressed();

    void Stop(bool wait);
    void Start();

private:
    bool _OnClose(GdkEventAny* event);

    void _OnHidden();
    void _OnShown();

    void _UpdateButtonState();
    void _OnSelectedPathChanged();

    void _RunTaskThread();

private:
    Gtk::MenuButton* Menu;

    // Primary menu
    PrimaryMenu MenuPopover;

    Gtk::Button* AlreadyImportedStartStopButton;
    Gtk::FileChooserButton* AlreadyImportedCheckPathChooser;
    Gtk::Spinner* AlreadyImportedProcessingSpinner;
    Gtk::Label* AlreadyImportedStatusLabel;
    Gtk::Label* AlreadyImportedFilesCheckedLabel;

    std::string TargetFolderToProcess;

    std::atomic<bool> StopProcessing{true};
    std::thread TaskThread;
    bool ThreadJoined = true;

    int TotalItemsProcessed = 0;
    int TotalItemsDeleted = 0;
    int TotalItemsCopiedToRepairCollection = 0;
};

} // namespace DV
