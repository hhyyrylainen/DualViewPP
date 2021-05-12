// ------------------------------------ //
#include "MaintenanceTools.h"

#include "Database.h"
#include "DualView.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //
MaintenanceTools::MaintenanceTools(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &MaintenanceTools::_OnClose));

    signal_unmap().connect(sigc::mem_fun(*this, &MaintenanceTools::_OnHidden));

    // Get and apply primary menu options
    BUILDER_GET_PRIMARY_MENU_NAMED("MenuButtonMaintenance", Menu, MenuPopover);
}

MaintenanceTools::~MaintenanceTools() {}
// ------------------------------------ //
bool MaintenanceTools::_OnClose(GdkEventAny* event)
{
    // Just hide it //
    hide();
    return true;
}

void MaintenanceTools::_OnHidden() {}
// ------------------------------------ //
