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
    Gtk::Box mainContainer;
    Gtk::ScrolledWindow listScroll;
    Gtk::Box listContainer;

    Gtk::Label nothingToShow;
};

} // namespace DV
