#pragma once

#include "BaseWindow.h"
#include "IsAlive.h"

#include "components/PrimaryMenu.h"
#include "components/SuperContainer.h"
#include "components/SuperViewer.h"

#include <gtkmm.h>

#include <atomic>

namespace DV {

class Collection;

//! \brief Allows user to reorder images in a collection
class ReorderWindow : public BaseWindow, public Gtk::Window, public IsAlive {
public:
    ReorderWindow(const std::shared_ptr<Collection>& collection);
    ~ReorderWindow();

protected:
    void _OnClose() override;

private:
    // Titlebar widgets
    Gtk::HeaderBar HeaderBar;
    Gtk::MenuButton Menu;
    Gtk::MenuButton Undo;
    Gtk::MenuButton Redo;

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
    Gtk::Button Apply;
    Gtk::Box BottomButtons;
};

} // namespace DV
