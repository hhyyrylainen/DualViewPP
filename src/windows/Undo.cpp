// ------------------------------------ //
#include "Undo.h"

using namespace DV;
// ------------------------------------ //
UndoWindow::UndoWindow() : nothingToShow("No history items available")
{
    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));

    property_resizable() = true;
    property_title() = "DualView - Undo Recent Actions";

    // Default size if empty
    property_width_request() = 500;
    property_height_request() = 300;

    // add(nothingToShow);

    mainContainer.property_orientation() = Gtk::ORIENTATION_VERTICAL;
    mainContainer.property_vexpand() = true;
    mainContainer.property_hexpand() = true;
    add(mainContainer);

    listScroll.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_ALWAYS);
    listScroll.property_vexpand() = true;
    listScroll.property_hexpand() = true;
    mainContainer.add(listScroll);

    listContainer.property_orientation() = Gtk::ORIENTATION_VERTICAL;
    listContainer.property_vexpand() = true;
    listContainer.property_hexpand() = true;
    listScroll.add(listContainer);

    nothingToShow.property_halign() = Gtk::ALIGN_CENTER;
    nothingToShow.property_hexpand() = true;
    nothingToShow.property_valign() = Gtk::ALIGN_CENTER;
    nothingToShow.property_vexpand() = true;
    listContainer.add(nothingToShow);

    show_all();
}

UndoWindow::~UndoWindow()
{
    Close();
}

void UndoWindow::_OnClose() {}
// ------------------------------------ //
