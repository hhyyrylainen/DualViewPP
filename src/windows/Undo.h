#pragma once

#include "BaseWindow.h"
#include "IsAlive.h"

#include "Common/BaseNotifiable.h"

#include <gtkmm.h>

#include <atomic>

namespace DV {

//! \brief Manages letting the user undo and redo actions and edit them
class UndoWindow : public BaseWindow, public Gtk::Window, public IsAlive {
public:
    UndoWindow();
    ~UndoWindow();

protected:
    void _OnClose() override;

private:
    // Titlebar widgets
    Gtk::HeaderBar HeaderBar;
    Gtk::MenuButton Menu;
    Gtk::Button SearchButton;

    // Main content area
    Gtk::Box MainContainer;
    Gtk::ScrolledWindow ListScroll;
    Gtk::Box ListContainer;

    // Loading widgets
    Gtk::Label NothingToShow;
};

} // namespace DV
