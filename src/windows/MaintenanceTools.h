#pragma once

#include "BaseWindow.h"
#include "IsAlive.h"

#include "Common.h"

#include "components/PrimaryMenu.h"

#include <gtkmm.h>

#include <atomic>
#include <functional>
#include <thread>

namespace DV {

//! \brief Tool window with various maintenance actions
class MaintenanceTools : public Gtk::Window, public IsAlive {
public:
    MaintenanceTools(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~MaintenanceTools();

    void Stop(bool wait);

    void StartImageExistCheck();
    void StartDeleteThumbnails();
    void StartDeleteOrphanedResources();
    void StartFixOrphanedResources();

private:
    bool _OnClose(GdkEventAny* event);

    void _OnHidden();
    void _OnShown();

    void _InsertTextResult(const std::string& text);
    void _InsertBrokenImageResult(
        DBID image, const std::string& brokenPath, const std::string& autoFix);
    bool _LimitResults();
    void _ClearResultsPressed();
    void _CancelPressed();

    void DeleteImportedStagingFolderItemsClicked();

    void _UpdateState();
    void _OnTaskFinished();

    void _RunTaskThread(const std::function<void()>& operation);
    void _RunFileExistCheck();
    void _RunDeleteThumbnails();
    void _RunDeleteOrphaned();
    void _RunFixOrphaned();

private:
    Gtk::MenuButton* Menu;

    // Primary menu
    PrimaryMenu MenuPopover;

    Gtk::Button* MaintenanceCancelButton;
    Gtk::Button* CheckAllExist;
    Gtk::Button* DeleteAllThumbnails;
    Gtk::Button* FixOrphanedResources;
    Gtk::Button* FixOrphanedImages;
    Gtk::Button* DeleteImportedStagingFolderItems;

    Gtk::Box* MaintenanceResults;
    std::vector<std::shared_ptr<Gtk::Widget>> MaintenanceResultWidgets;
    Gtk::Button* MaintenanceClearResults;

    Gtk::Spinner* MaintenanceSpinner;
    Gtk::Label* MaintenanceStatusLabel;
    Gtk::ProgressBar* MaintenanceProgressBar;

    bool HasRunSomething = false;
    bool TaskRunning = false;

    std::atomic<bool> RunTaskThread{false};
    std::thread TaskThread;

    std::atomic<float> ProgressFraction;
};

} // namespace DV
