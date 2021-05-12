#pragma once

#include "BaseWindow.h"
#include "IsAlive.h"

#include "components/PrimaryMenu.h"

#include <gtkmm.h>

namespace DV {

//! \brief Tool window with various maintenance actions
class MaintenanceTools : public Gtk::Window, public IsAlive {
public:
    MaintenanceTools(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~MaintenanceTools();

private:
    bool _OnClose(GdkEventAny* event);

    void _OnHidden();

private:
    Gtk::MenuButton* Menu;

    // Primary menu
    PrimaryMenu MenuPopover;
};

} // namespace DV
