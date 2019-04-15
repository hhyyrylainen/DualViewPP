#pragma once

#include "BaseWindow.h"
#include "IsAlive.h"

#include "ReversibleAction.h"
#include "SignatureCalculator.h"

#include "components/PrimaryMenu.h"
#include "components/SuperContainer.h"
#include "components/SuperViewer.h"

#include <gtkmm.h>

#include <atomic>

namespace DV {

class DatabaseAction;

//! \brief Manages letting the user undo and redo actions and edit them
class DuplicateFinderWindow final : public BaseWindow, public Gtk::Window, public IsAlive {
    enum class ACTION_TYPE { Remove, Merge, NotDuplicate };

    class HistoryItem final : public ReversibleAction {
        friend DuplicateFinderWindow;

    public:
        HistoryItem(DuplicateFinderWindow& target,
            const std::vector<std::shared_ptr<Image>>& removedimages,
            size_t groupsvectorindextoremoveat, ACTION_TYPE type);

        bool Redo() override;
        bool Undo() override;

        std::vector<std::tuple<DBID, DBID>> GenerateIgnorePairs() const;

    protected:
        int StoredShownDuplicateGroup = -1;
        std::vector<std::shared_ptr<Image>> RemovedImages;
        size_t GroupsVectorIndexToRemoveAt;

        // Things for applying the effects and storing info
        DuplicateFinderWindow& Target;

        ACTION_TYPE Type;

        //! For Types that perform additional actions that is stored here for undo purposes
        std::shared_ptr<DatabaseAction> AdditionalAction;

        //! This is used when an entire group is removed to restore extra items not in
        //! RemovedImages
        std::vector<std::shared_ptr<Image>> ExtraRemovedGroupImages;
    };

public:
    DuplicateFinderWindow();
    ~DuplicateFinderWindow();

    bool PerformAction(HistoryItem& action);
    bool UndoAction(HistoryItem& action);

    void ResetState();

protected:
    void _OnClose() override;

private:
    void _ScanButtonPressed();
    void _SkipPressed();
    void _UndoPressed();
    void _RedoPressed();
    void _DeleteSelectedAfterFirstPressed();
    void _DeleteAllAfterFirstPressed();
    void _NotDuplicatesPressed();
    void _ClearNotDuplicatesPressed();
    void _ApplyPrimaryMenuSettings();

    void _MergeCurrentGroupDuplicates(const std::vector<std::shared_ptr<Image>>& tomerge);

    //! \brief Check the status of signature calculation and queue the database lookup for
    //! duplicates
    void _CheckScanStatus();

    //! \brief Called from SignatureCalculator to update stats.
    //!
    //! This also periodically calls _CheckScanstatus
    void _ReportSignatureCalculationStatus(int processed, int total, bool done);

    //! \brief Updates the label describing duplicates and the duplicate handling widgets
    void _UpdateDuplicateWidgets();

    //! \brief Adds new found duplicates
    void _DetectNewDuplicates(
        const std::map<DBID, std::vector<std::tuple<DBID, int>>>& duplicates);

    void _BrowseDuplicates(int newindex);

    void _UpdateUndoRedoButtons();

private:
    // Titlebar widgets
    Gtk::HeaderBar HeaderBar;
    Gtk::MenuButton Menu;
    Gtk::Button Undo;
    Gtk::Button Redo;
    Gtk::Button ScanControl;

    // Primary menu
    PrimaryMenu MenuPopover;
    Gtk::Button ResetResults;
    Gtk::Button ClearNotDuplicates;
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

    //! This prevents sending the images again. Detecting images without signatures once is
    //! good enough
    bool ImagesMissingSignaturesCalculated = false;

    // Resources for the duplicate groups
    bool QueryingDBForDuplicates = false;
    bool NoMoreQueryResults = false;

    //! Currently found duplicates that need the user to resolve them
    std::vector<std::vector<std::shared_ptr<Image>>> DuplicateGroups;

    //! True when a DB fetch for Image objects that are duplicates is happening
    bool FetchingNewDuplicateGroups = false;

    //! The total amount of DuplicateGroups found to keep consistent group count
    size_t TotalGroupsFound = 0;

    //! The selected duplicate group (handled groups are removed from DuplicateGroups so this
    //! is usually 0 unless the user is browsing around between the groups)
    int ShownDuplicateGroup = -1;

    //! Image delete action history to allow going back
    ActionHistory History;
};

} // namespace DV
