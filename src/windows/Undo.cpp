// ------------------------------------ //
#include "Undo.h"

using namespace DV;
// ------------------------------------ //
UndoWindow::UndoWindow() :
    ClearHistory("Clear History"), HistorySizeLabel("History items to keep"),
    NothingToShow("No history items available")
{
    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));

    set_default_size(500, 300);
    property_resizable() = true;

    Menu.set_image_from_icon_name("open-menu-symbolic");

    // Window specific controls
    ClearHistory.property_relief() = Gtk::RELIEF_NONE;
    MenuPopover.Container.pack_start(ClearHistory);
    MenuPopover.Container.pack_start(Separator1);
    MenuPopover.Container.pack_start(HistorySizeLabel);

    HistorySize.property_editable() = true;
    HistorySize.property_input_purpose() = Gtk::INPUT_PURPOSE_NUMBER;
    HistorySize.property_snap_to_ticks() = true;
    HistorySize.property_snap_to_ticks() = true;
    HistorySize.set_range(0, 250);
    HistorySize.set_increments(1, 10);
    HistorySize.set_digits(0);
    // TODO: implement logic
    HistorySize.set_value(50);

    MenuPopover.Container.pack_start(HistorySize);

    MenuPopover.show_all_children();

    Menu.set_popover(MenuPopover);

    SearchButton.set_image_from_icon_name("edit-find-symbolic");

    HeaderBar.property_title() = "Latest Actions";
    HeaderBar.property_show_close_button() = true;
    HeaderBar.pack_end(Menu);
    HeaderBar.pack_end(SearchButton);
    set_titlebar(HeaderBar);


    MainContainer.property_orientation() = Gtk::ORIENTATION_VERTICAL;
    MainContainer.property_vexpand() = true;
    MainContainer.property_hexpand() = true;
    add(MainContainer);

    ListScroll.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_ALWAYS);
    ListScroll.property_vexpand() = true;
    ListScroll.property_hexpand() = true;
    MainContainer.add(ListScroll);

    ListContainer.property_orientation() = Gtk::ORIENTATION_VERTICAL;
    ListContainer.property_vexpand() = true;
    ListContainer.property_hexpand() = true;
    ListScroll.add(ListContainer);

    NothingToShow.property_halign() = Gtk::ALIGN_CENTER;
    NothingToShow.property_hexpand() = true;
    NothingToShow.property_valign() = Gtk::ALIGN_CENTER;
    NothingToShow.property_vexpand() = true;
    ListContainer.add(NothingToShow);

    show_all_children();
}

UndoWindow::~UndoWindow()
{
    Close();
}

void UndoWindow::_OnClose() {}
// ------------------------------------ //
