// ------------------------------------ //
#include "DLListItem.h"

#include "core/DualView.h"

using namespace DV;
// ------------------------------------ //

DLListItem::DLListItem(std::shared_ptr<NetGallery> todownload) :
    Gallery(todownload),
    Container(Gtk::ORIENTATION_HORIZONTAL),
    URLLabel("URL not loaded..."),
    ButtonBox(Gtk::ORIENTATION_VERTICAL),
    AdvancedSettings("Advanced Settings"),
    Delete("Delete")
{
    set_hexpand(false);
    add(Container);
    
    Container.pack_start(Enabled, false, false);
    Enabled.set_state(false);
    Enabled.set_valign(Gtk::ALIGN_CENTER);

    Container.pack_start(URLLabel, false, true);
    URLLabel.set_margin_left(5);

    Container.pack_start(Active, false, false);

    Container.pack_start(Progress, true, true);
    Progress.set_valign(Gtk::ALIGN_CENTER);
    Progress.set_size_request(30, 25);

    Container.pack_start(ErrorLabel, false, true);

    Container.pack_end(ButtonBox, true, false);
    ButtonBox.set_valign(Gtk::ALIGN_CENTER);

    ButtonBox.add(AdvancedSettings);
    ButtonBox.add(Delete);

    Container.pack_end(NameBox, false, true);
    NameBox.set_valign(Gtk::ALIGN_CENTER);

    if(Gallery)
        ReadGalleryData();

    show_all_children();
}

DLListItem::~DLListItem(){


}
// ------------------------------------ //
void DLListItem::ReadGalleryData(){

    DualView::IsOnMainThreadAssert();

    
}
