// ------------------------------------ //
#include "DLListItem.h"

#include "resources/NetGallery.h"

#include "Common.h"
#include "DualView.h"

using namespace DV;
// ------------------------------------ //
DLListItem::DLListItem(std::shared_ptr<NetGallery> todownload) :
    Gallery(todownload), Container(Gtk::ORIENTATION_HORIZONTAL), URLLabel("URL not loaded..."),
    ButtonBox(Gtk::ORIENTATION_VERTICAL), AdvancedSettings("Advanced Settings"),
    Delete("Delete")
{
    set_hexpand(false);
    add(Container);

    Container.pack_start(Enabled, false, false);
    Enabled.set_state(false);
    Enabled.set_valign(Gtk::ALIGN_CENTER);

    Container.pack_start(URLLabel, false, true);
    URLLabel.property_margin_start() = 5;
    URLLabel.set_ellipsize(Pango::ELLIPSIZE_MIDDLE);

    Container.pack_start(Active, false, false);

    Container.pack_start(Progress, true, true);
    Progress.set_valign(Gtk::ALIGN_CENTER);
    Progress.set_size_request(30, 25);

    Container.pack_start(ErrorLabel, false, true);

    Container.pack_end(ButtonBox, false, false);
    ButtonBox.set_valign(Gtk::ALIGN_CENTER);

    AdvancedSettings.signal_clicked().connect(
        sigc::mem_fun(*this, &DLListItem::OpenEditorForDownload));

    ButtonBox.add(AdvancedSettings);
    ButtonBox.add(Delete);

    Delete.signal_clicked().connect(sigc::mem_fun(*this, &DLListItem::OnPressedRemove));

    Container.pack_end(NameBox, true, true);
    NameBox.set_valign(Gtk::ALIGN_CENTER);
    NameBox.signal_changed().connect(sigc::mem_fun(*this, &DLListItem::OnNameUpdated));

    show_all_children();

    if(Gallery)
        ReadGalleryData();
}

DLListItem::~DLListItem() {}
// ------------------------------------ //
void DLListItem::OpenEditorForDownload()
{
    DualView::Get().OpenDownloadItemEditor(Gallery);
}
// ------------------------------------ //
void DLListItem::SetProgress(float value)
{
    auto alive = GetAliveMarker();

    DualView::Get().RunOnMainThread([=]() {
        INVOKE_CHECK_ALIVE_MARKER(alive);

        Progress.set_value(value);
    });
}
// ------------------------------------ //
void DLListItem::ReadGalleryData()
{
    auto alive = GetAliveMarker();

    DualView::Get().InvokeFunction([=]() {
        INVOKE_CHECK_ALIVE_MARKER(alive);

        GUARD_LOCK();

        LOG_INFO("Settings DLListItem data");

        if(!IsConnectedTo(Gallery.get(), guard))
            ConnectToNotifier(guard, Gallery.get());

        URLLabel.set_text(Gallery->GetGalleryURL());
        Progress.set_value(0);
        Enabled.set_state(false);
        NameBox.set_text(Gallery->GetTargetGalleryName());

        ErrorLabel.set_text("");

        LOG_INFO("Finished DLListItem data update");
    });
}
// ------------------------------------ //
bool DLListItem::IsSelected() const
{
    DualView::IsOnMainThreadAssert();
    return Enabled.get_state();
}

void DLListItem::SetSelected()
{
    DualView::IsOnMainThreadAssert();

    if(!Enabled.get_sensitive())
        return;

    Enabled.set_state(true);
}

void DLListItem::LockSelected(bool locked)
{
    auto alive = GetAliveMarker();

    DualView::Get().RunOnMainThread([=]() {
        INVOKE_CHECK_ALIVE_MARKER(alive);

        Enabled.set_sensitive(!locked);
    });
}
// ------------------------------------ //
void DLListItem::OnNameUpdated()
{
    DualView::IsOnMainThreadAssert();

    if(Gallery && Gallery->GetTargetGalleryName() != NameBox.get_text()) {

        Gallery->SetTargetGalleryName(NameBox.get_text());
    }
}

void DLListItem::OnNotified(
    Lock& ownlock, Leviathan::BaseNotifierAll* parent, Lock& parentlock)
{
    LOG_INFO("DLListItem: gallery changed, reading changes");
    ReadGalleryData();
}
// ------------------------------------ //
void DLListItem::SetRemoveCallback(std::function<void(DLListItem&)> callback)
{
    OnRemoveCallback = callback;
}

void DLListItem::OnPressedRemove()
{
    if(OnRemoveCallback)
        OnRemoveCallback(*this);
}
