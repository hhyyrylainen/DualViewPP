#pragma once

#include "BaseWindow.h"
#include "IsAlive.h"

#include "components/PrimaryMenu.h"
#include "components/SuperContainer.h"


#include "Common/BaseNotifiable.h"

#include <gtkmm.h>

#include <atomic>

namespace DV {

class DatabaseAction;
class ResourceWithPreview;

//! \brief Shows a single action
class ActionDisplay : public Gtk::Frame, public IsAlive {
public:
    ActionDisplay(const std::shared_ptr<DatabaseAction>& action);
    ~ActionDisplay();

    void RefreshData();

private:
    void _OnDataRetrieved(const std::string& description,
        const std::vector<std::shared_ptr<ResourceWithPreview>>& previewitems);

private:
    std::shared_ptr<DatabaseAction> Action;

    Gtk::Box MainBox;

    Gtk::Label Description;
    Gtk::Frame ContainerFrame;
    SuperContainer ResourcePreviews;
    Gtk::Box LeftSide;

    Gtk::Box RightSide;
    Gtk::Button UndoRedoButton;
    Gtk::Button EditButton;
};

//! \brief Manages letting the user undo and redo actions and edit them
class UndoWindow : public BaseWindow, public Gtk::Window, public IsAlive {
public:
    UndoWindow();
    ~UndoWindow();

    //! \brief Clears the found actions and the widgets showing them FoundActions
    void Clear();

protected:
    void _OnClose() override;

    bool _StartSearchFromKeypress(GdkEventKey* event);

    void _SearchUpdated();

    void _FinishedQueryingDB(const std::vector<std::shared_ptr<DatabaseAction>>& actions);

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
    //! Updates the button status from the search bar visibility and vice versa
    Glib::RefPtr<Glib::Binding> SearchActiveBinding;
    Gtk::SearchEntry Search;
    Gtk::Overlay MainArea;
    Gtk::Spinner QueryingDatabase;
    Gtk::ScrolledWindow ListScroll;
    Gtk::Box ListContainer;

    // Loading widgets
    Gtk::Label NothingToShow;

    // Loaded action widgets
    std::vector<std::shared_ptr<ActionDisplay>> FoundActions;
};

} // namespace DV
