// ------------------------------------ //
#include "PrimaryMenu.h"

#include "UtilityHelpers.h"

#include "DualView.h"

using namespace DV;
// ------------------------------------ //
PrimaryMenu::PrimaryMenu() : ShowMain("Show _Main Window", true), About("_About", true)
{
    ShowMain.property_relief() = Gtk::RELIEF_NONE;
    About.property_relief() = Gtk::RELIEF_NONE;

    Container.set_orientation(Gtk::ORIENTATION_VERTICAL);

    Container.pack_start(ShowMain);
    Container.pack_start(Separator1);

    Container.pack_end(About);
    Container.pack_end(Separator2);

    add(Container);

    ShowMain.signal_clicked().connect(sigc::mem_fun(*this, &PrimaryMenu::_OpenMain));
    About.signal_clicked().connect(sigc::mem_fun(*this, &PrimaryMenu::_OpenAbout));

    show_all_children();
}

PrimaryMenu::~PrimaryMenu() {}
// ------------------------------------ //
void PrimaryMenu::_OpenMain()
{
    DualView::Get().OpenMainWindow();
    hide();
}
void PrimaryMenu::_OpenAbout()
{
    Gtk::Window* parent = dynamic_cast<Gtk::Window*>(this->get_toplevel());

    hide();

    if(parent) {
        Gtk::Dialog dialog("About | DualView++", *parent, true);
        auto* content = dialog.get_content_area();
        Gtk::Label label;

        label.set_max_width_chars(60);
        label.set_markup(
            LoadResourceCopy("/com/boostslair/dualviewpp/resources/about_text.txt"));

        Gtk::Button ok("_Close", true);
        dialog.add_action_widget(ok, 1);
        // This gives an error
        // dialog.set_default(ok);
        content->add(label);
        dialog.show_all_children();
        dialog.run();
    }
}
