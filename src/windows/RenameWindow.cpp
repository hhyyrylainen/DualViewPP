// ------------------------------------ //
#include "RenameWindow.h"

#include "DualView.h"

#include <canberra-gtk.h>

using namespace DV;
// ------------------------------------ //
RenameWindow::RenameWindow(
    const std::string& originalName, ApplyMethod applyNewName, VerifyMethod verifyNewName) :
    OriginalName(originalName),
    ApplyNew(std::move(applyNewName)), Verifier(std::move(verifyNewName)),
    MainBox(Gtk::ORIENTATION_VERTICAL), ApplyButton("Apply")
{
    ApplyButton.get_style_context()->add_class("suggested-action");

    MainBox.pack_end(ApplyButton, false, true);
    MainBox.pack_start(TextEntry, false, true);
    MainBox.pack_start(ErrorMessage, false, true);

    add(MainBox);

    ApplyButton.signal_clicked().connect(sigc::mem_fun(this, &RenameWindow::_OnApply));

    TextEntry.set_can_default(true);
    TextEntry.set_text(originalName.c_str());
    TextEntry.signal_changed().connect(sigc::mem_fun(this, &RenameWindow::_OnTextChanged));
    TextEntry.signal_activate().connect(sigc::mem_fun(this, &RenameWindow::_OnApply));
    TextEntry.set_placeholder_text("Enter new name");

    show_all_children();

    set_default_size(400, 90);
    set_title("DualView++ - Rename Item");
}

RenameWindow::~RenameWindow()
{
    Close();
}
// ------------------------------------ //
bool RenameWindow::IsNewNameValid(const std::string& name)
{
    const auto result = Verifier(name);

    auto isalive = GetAliveMarker();

    DualView::Get().RunOnMainThread([this, isalive, text = std::get<1>(result)]() {
        INVOKE_CHECK_ALIVE_MARKER(isalive);

        ErrorMessage.set_text(text.c_str());
    });

    return std::get<0>(result);
}
// ------------------------------------ //
void RenameWindow::_OnClose() {}
// ------------------------------------ //
void RenameWindow::_OnApply()
{
    set_sensitive(false);
    auto window = get_window();
    if(window)
        window->set_cursor(Gdk::Cursor::create(window->get_display(), Gdk::WATCH));

    std::string name(TextEntry.get_text().c_str());

    if(!IsNewNameValid(name)) {
        ca_gtk_play_for_widget(
            ((Gtk::Widget*)this)->gobj(), 0, CA_PROP_EVENT_ID, "dialog-error", NULL);
        set_sensitive(true);
        window->set_cursor();
        return;
    }

    const auto result = ApplyNew(name);

    if(!std::get<0>(result)) {

        auto dialog =
            Gtk::MessageDialog(*this, "Failed to apply new name", false, Gtk::MESSAGE_ERROR);
        dialog.set_secondary_text("Additional information: ");
        dialog.run();

        window->set_cursor();
        set_sensitive(true);
        return;
    }

    Close();
}
// ------------------------------------ //
void RenameWindow::_OnTextChanged()
{
    std::string name(TextEntry.get_text().c_str());
    auto isalive = GetAliveMarker();

    DualView::Get().QueueDBThreadFunction([this, isalive, name]() {
        const auto valid = IsNewNameValid(name);

        DualView::Get().InvokeFunction([this, isalive, valid]() {
            INVOKE_CHECK_ALIVE_MARKER(isalive);

            if(valid) {
                TextEntry.get_style_context()->remove_class("Invalid");
            } else {
                TextEntry.get_style_context()->add_class("Invalid");
            }
        });
    });
}
