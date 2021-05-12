// ------------------------------------ //
#include "AlreadyImportedImageDeleter.h"

#include "Database.h"
#include "DualView.h"

#include "Common.h"

#include <boost/filesystem.hpp>

using namespace DV;
// ------------------------------------ //
AlreadyImportedImageDeleter::AlreadyImportedImageDeleter(
    _GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    signal_delete_event().connect(
        sigc::mem_fun(*this, &AlreadyImportedImageDeleter::_OnClose));

    signal_unmap().connect(sigc::mem_fun(*this, &AlreadyImportedImageDeleter::_OnHidden));

    // Get and apply primary menu options
    BUILDER_GET_PRIMARY_MENU_NAMED("MenuButtonAlreadyImported", Menu, MenuPopover);

    /*
    Gtk::Button* MakeBusy;

    BUILDER_GET_WIDGET(MakeBusy);

    MakeBusy->signal_clicked().connect(
        sigc::mem_fun(*this, &AlreadyImportedImageDeleter::OnMakeDBBusy));

    Gtk::Button* TestImageRead;

    BUILDER_GET_WIDGET(TestImageRead);

    TestImageRead->signal_clicked().connect(
        sigc::mem_fun(*this, &AlreadyImportedImageDeleter::OnTestImageRead));


    Gtk::Button* TestInstanceCreation;

    BUILDER_GET_WIDGET(TestInstanceCreation);

    TestInstanceCreation->signal_clicked().connect(
        sigc::mem_fun(*this, &AlreadyImportedImageDeleter::OnTestInstanceCreation));
    */
}

AlreadyImportedImageDeleter::~AlreadyImportedImageDeleter() {}
// ------------------------------------ //
bool AlreadyImportedImageDeleter::_OnClose(GdkEventAny* event)
{
    // Just hide it //
    hide();
    return true;
}

void AlreadyImportedImageDeleter::_OnHidden() {}
// ------------------------------------ //
void AlreadyImportedImageDeleter::OnStartStopPressed()
{

}
// ------------------------------------ //
