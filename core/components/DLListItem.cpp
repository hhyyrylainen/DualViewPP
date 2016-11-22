// ------------------------------------ //
#include "DLListItem.h"

#include "core/resources/NetGallery.h"

#include "core/DualView.h"
#include "Common.h"

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
    URLLabel.set_ellipsize(Pango::ELLIPSIZE_MIDDLE);

    Container.pack_start(Active, false, false);

    Container.pack_start(Progress, true, true);
    Progress.set_valign(Gtk::ALIGN_CENTER);
    Progress.set_size_request(30, 25);

    Container.pack_start(ErrorLabel, false, true);

    Container.pack_end(ButtonBox, false, false);
    ButtonBox.set_valign(Gtk::ALIGN_CENTER);

    ButtonBox.add(AdvancedSettings);
    ButtonBox.add(Delete);

    Container.pack_end(NameBox, true, true);
    NameBox.set_valign(Gtk::ALIGN_CENTER);

    if(Gallery)
        ReadGalleryData();

    show_all_children();
}

DLListItem::~DLListItem(){


}
// ------------------------------------ //
void DLListItem::SetProgress(float value){

    auto alive = GetAliveMarker();

    DualView::Get().RunOnMainThread([=](){

            INVOKE_CHECK_ALIVE_MARKER(alive);

            Progress.set_value(value);
        });
}
// ------------------------------------ //
void DLListItem::ReadGalleryData(){

    DualView::IsOnMainThreadAssert();

    URLLabel.set_text(Gallery->GetGalleryURL());
    Progress.set_value(0);
    Enabled.set_state(false);
    NameBox.set_text(Gallery->GetTargetGalleryName());

    ErrorLabel.set_text("");
}
