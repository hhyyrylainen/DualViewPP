#pragma once

#include "BaseWindow.h"
#include "IsAlive.h"

#include "ReversibleAction.h"

#include "components/PrimaryMenu.h"
#include "components/SuperContainer.h"
#include "components/SuperViewer.h"

#include <gtkmm.h>

#include <atomic>

namespace DV {

class Collection;

//! \brief Allows user to reorder images in a Collection
class ReorderWindow : public BaseWindow, public Gtk::Window, public IsAlive {
    enum class MOVE_GROUP { Workspace, MainList };

    class HistoryItem final : public ReversibleAction {
        friend ReorderWindow;

    public:
        HistoryItem(ReorderWindow& target, MOVE_GROUP movedfrom,
            const std::vector<std::shared_ptr<Image>>& imagestomove, MOVE_GROUP moveto,
            size_t movetargetindex);

    protected:
        bool DoRedo() override;
        bool DoUndo() override;

    protected:
        ReorderWindow& Target;

        MOVE_GROUP MovedFrom;
        //! This is calculated on Redo
        std::vector<size_t> MovedFromIndex;

        std::vector<std::shared_ptr<Image>> ImagesToMove;

        MOVE_GROUP MoveTo;
        size_t MoveTargetIndex;

        //! This is an extra place to stash replaced inactive images in the main list
        std::vector<std::tuple<size_t, std::shared_ptr<Image>>> ReplacedInactive;
    };

public:
    ReorderWindow(const std::shared_ptr<Collection>& collection);
    ~ReorderWindow();

    //! \brief Applies the changes and closes this window
    void Apply();

    void Reset();

    std::vector<std::shared_ptr<Image>> GetSelected() const;
    std::vector<std::shared_ptr<Image>> GetSelectedInWorkspace() const;

    bool PerformAction(HistoryItem& action);
    bool UndoAction(HistoryItem& action);

protected:
    void _OnClose() override;

    bool _OnClosed(GdkEventAny* event);

private:
    std::vector<std::shared_ptr<Image>>& _GetCollectionForMoveGroup(MOVE_GROUP group);
    void _UpdateListsTouchedByAction(HistoryItem& action);
    //! \todo This could be moved to a helper file
    static std::vector<std::shared_ptr<Image>> _CastToImages(
        std::vector<std::shared_ptr<ResourceWithPreview>>& items);

    void _UpdateButtonStatus();
    void _UpdateShownItems();
    void _UpdateShownWorkspaceItems();

    void _OpenSelectedInImporterPressed();
    void _DeleteSelectedPressed();
    void _MoveToWorkspacePressed();
    void _MoveBackFromWorkspacePressed();
    void _UndoPressed();
    void _RedoPressed();
    void _SelectAllPressed();

private:
    // Titlebar widgets
    Gtk::HeaderBar HeaderBar;
    Gtk::MenuButton Menu;
    Gtk::Button Undo;
    Gtk::Button Redo;

    // Primary menu
    PrimaryMenu MenuPopover;
    Gtk::Button ResetResults;

    // Window contents
    Gtk::Box MainContainer;

    // Top area
    Gtk::Label WorkspaceLabel;
    Gtk::Button SelectAllInWorkspace;
    Gtk::Box VeryTopLeftContainer;
    Gtk::Frame WorkspaceFrame;
    SuperContainer Workspace;
    Gtk::Box TopLeftSide;
    Gtk::Label LastSelectedLabel;
    SuperViewer LastSelectedImage;
    Gtk::Box TopRightSide;

    // Middle buttons
    Gtk::Label CurrentImageOrder;
    Gtk::Button DownArrow;
    Gtk::Button UpArrow;
    Gtk::Box MiddleBox;

    // Includes the buttons to align the right side
    Gtk::Box TopContainer;

    // Image list part
    Gtk::Frame ImageListFrame;
    SuperContainer ImageList;

    // Bottom buttons
    Gtk::Button RemoveSelected;
    Gtk::Button OpenSelectedInImporter;
    Gtk::Button ApplyButton;
    Gtk::Box BottomButtons;

    // Other resources
    bool DoneChanges = false;
    const std::shared_ptr<Collection> TargetCollection;
    std::vector<std::shared_ptr<Image>> CollectionImages;
    std::vector<std::shared_ptr<Image>> WorkspaceImages;

    //! Used to always properly apply the inactive status to right items
    std::vector<std::shared_ptr<Image>> InactiveItems;

    //! Undo / Redo
    ActionHistory History;
};

} // namespace DV
