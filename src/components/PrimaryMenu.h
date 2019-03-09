#pragma once

#include <gtkmm.h>

namespace DV {

//! \brief Primary menu for all dualview windows
//!
//! Can be customized per window type
class PrimaryMenu : public Gtk::Popover {
public:
    PrimaryMenu();
    ~PrimaryMenu();

protected:
    void _OpenMain();
    void _OpenAbout();

public:
    Gtk::Box Container;

protected:
    Gtk::Button ShowMain;
    Gtk::Button About;

    Gtk::Separator Separator1;
    Gtk::Separator Separator2;
};
} // namespace DV
