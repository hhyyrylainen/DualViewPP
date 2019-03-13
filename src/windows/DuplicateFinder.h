#pragma once

#include "BaseWindow.h"
#include "IsAlive.h"

#include "SignatureCalculator.h"

#include "components/PrimaryMenu.h"
#include "components/SuperContainer.h"
#include "components/SuperViewer.h"

#include <gtkmm.h>

#include <atomic>

namespace DV {

//! \brief Manages letting the user undo and redo actions and edit them
class DuplicateFinderWindow : public BaseWindow, public Gtk::Window, public IsAlive {
public:
    DuplicateFinderWindow();
    ~DuplicateFinderWindow();

protected:
    void _OnClose() override;

    void _ScanButtonPressed();

    //! \brief Check the status of signature calculation and queue the database lookup for
    //! duplicates
    void _CheckScanStatus();

    //! \brief Called from SignatureCalculator to update stats.
    //!
    //! This also periodically calls _CheckScanstatus
    void _ReportSignatureCalculationStatus(int processed, int total, bool done);

    //! \brief Updates the label describing
    void _UpdateDuplicateStatusLabel();

private:
    // Titlebar widgets
    Gtk::HeaderBar HeaderBar;
    Gtk::MenuButton Menu;
    Gtk::MenuButton Undo;
    Gtk::MenuButton Redo;
    Gtk::Button ScanControl;

    // Primary menu
    PrimaryMenu MenuPopover;
    Gtk::Button ResetResults;
    Gtk::Separator Separator1;
    Gtk::Label SensitivityLabel;
    Gtk::Scale Sensitivity;

    // Window contents
    Gtk::Box MainContainer;

    // Progress area
    Gtk::Box ProgressContainer;
    Gtk::ProgressBar ScanProgress;
    Gtk::Label ProgressLabel;
    Gtk::Separator Separator2;

    // Resolve area
    Gtk::Label CurrentlyShownGroup;
    Gtk::Box ImagesContainer;
    Gtk::Box ImagesLeftSide;
    Gtk::Label FirstSelected;
    SuperViewer FirstImage;
    Gtk::Box ImagesRightSide;
    Gtk::Label LastSelected;
    SuperViewer LastImage;

    // Bottom part of resolve area
    Gtk::Box ImageListAreaContainer;
    Gtk::Box ImageListLeftSide;
    Gtk::Box ImageListLeftTop;
    Gtk::Label DuplicateImagesLabel;
    Gtk::Button DeleteSelectedAfterFirst;
    Gtk::Frame DuplicateGroupImagesFrame;
    SuperContainer DuplicateGroupImages;

    // Bottom right buttons
    Gtk::Box BottomRightContainer;
    Gtk::Button DeleteAllAfterFirst;
    Gtk::Button NotDuplicates;
    Gtk::Button Skip;

    // Other resources
    SignatureCalculator Calculator;
    bool DoneWithSignatures = false;

    // Is running the finding process
    bool Scanning = false;

    // Resources for the duplicate groups
    bool QueryingDBForDuplicates = false;
    bool NoMoreQueryResults = false;
    std::vector<std::vector<std::shared_ptr<Image>>> DuplicateGroups;
    size_t TotalGroupsFound = 0;
};

} // namespace DV
