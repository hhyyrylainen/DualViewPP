#pragma once

#include "BaseWindow.h"
#include "IsAlive.h"

#include "components/PrimaryMenu.h"

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

    void _ToggleSearch();
    void _SearchModeChanged();
    bool _StartSearchFromKeypress(GdkEventKey* event);

    void _SearchUpdated();

private:
    // Titlebar widgets
    Gtk::HeaderBar HeaderBar;
    Gtk::MenuButton Menu;
    Gtk::ToggleButton SearchButton;

    // Primary menu
    PrimaryMenu MenuPopover;
    Gtk::Button ClearHistory;
    Gtk::Separator Separator1;
    Gtk::Label HistorySizeLabel;
    Gtk::SpinButton HistorySize;

    // Main content area
    Gtk::Box MainContainer;
    Gtk::SearchBar SearchBar;
    Gtk::SearchEntry Search;
    Gtk::ScrolledWindow ListScroll;
    Gtk::Box ListContainer;

    // Loading widgets
    Gtk::Label NothingToShow;

    //! Prevent recursive shenanigans with showing the search bar
    bool SearchBarVisibilityUpdateHappening = false;
};

} // namespace DV
