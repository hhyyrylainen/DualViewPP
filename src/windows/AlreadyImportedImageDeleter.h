#pragma once

#include <atomic>
#include <thread>

#include <gtkmm.h>

#include "components/PrimaryMenu.h"

#include "BaseWindow.h"
#include "IsAlive.h"

namespace DV
{
//! \brief Tool for deleting images from a path that are already imported, helper to quickly
//! check if a lot of images are already imported or not
class AlreadyImportedImageDeleter : public Gtk::Window,
                                    public IsAlive
{
public:
    AlreadyImportedImageDeleter(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~AlreadyImportedImageDeleter() override;

    void OnStartStopPressed();

    void Stop(bool wait);
    void Start();

    void SetSelectedFolder(const std::string& path);

    [[nodiscard]] bool IsRunning() const noexcept
    {
        return !StopProcessing;
    }

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
