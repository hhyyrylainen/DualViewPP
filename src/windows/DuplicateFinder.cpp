// ------------------------------------ //
#include "DuplicateFinder.h"

using namespace DV;
// ------------------------------------ //
DuplicateFinderWindow::DuplicateFinderWindow()
{
    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));

    set_default_size(500, 300);
    property_resizable() = true;

    Menu.set_image_from_icon_name("open-menu-symbolic");

    HeaderBar.property_title() = "Duplicate Finder";
    HeaderBar.property_show_close_button() = true;
    HeaderBar.pack_end(Menu);
    set_titlebar(HeaderBar);

    show_all_children();
}

DuplicateFinderWindow::~DuplicateFinderWindow()
{
    Close();
}

void DuplicateFinderWindow::_OnClose() {}
// ------------------------------------ //
