#pragma once

#include "BaseWindow.h"
#include "IsAlive.h"

#include "components/PrimaryMenu.h"

#include <gtkmm.h>

namespace DV {

//! \brief Tool for deleting images from a path that are already imported, helper to quickly
//! check if a lot of images are already imported or not
class AlreadyImportedImageDeleter : public Gtk::Window, public IsAlive {
public:
    AlreadyImportedImageDeleter(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~AlreadyImportedImageDeleter();

    void OnStartStopPressed();

private:
    bool _OnClose(GdkEventAny* event);

    void _OnHidden();

private:
    Gtk::MenuButton* Menu;

    // Primary menu
    PrimaryMenu MenuPopover;
};

} // namespace DV
