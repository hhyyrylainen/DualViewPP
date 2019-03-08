#pragma once

#include "BaseWindow.h"
#include "IsAlive.h"

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

private:
    // Titlebar widgets
    Gtk::HeaderBar HeaderBar;
    Gtk::MenuButton Menu;
};

} // namespace DV
