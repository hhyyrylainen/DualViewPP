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
public:
    ReorderWindow(const std::shared_ptr<Collection>& collection);
    ~ReorderWindow();

    //! \brief Applies the changes and closes this window
    void Apply();

    void Reset();

    std::vector<std::shared_ptr<Image>> GetSelected() const;

protected:
    void _OnClose() override;

    bool _OnClosed(GdkEventAny* event);

    void _UpdateButtonStatus();

    void _OpenSelectedInImporterPressed();
    void _DeleteSelectedPressed();

    void _UpdateShownItems();

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

    //! Undo / Redo
    ActionHistory History;
};

} // namespace DV
