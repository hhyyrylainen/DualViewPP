#pragma once

#include <gtkmm.h>

namespace DV{

//! \brief Holds things about a collection of images that's ready to be downloaded
class DLListItem : public Gtk::Frame{
public:

    DLListItem();
    ~DLListItem();


protected:


protected:

    Gtk::Box Container;

    Gtk::Switch Enabled;
    Gtk::Spinner Active;
    Gtk::Label URLLabel;
    Gtk::LevelBar Progress;
    Gtk::Entry NameBox;

    Gtk::Label ErrorLabel;

    Gtk::Box ButtonBox;
    Gtk::Button AdvancedSettings;
    Gtk::Button Delete;
};

}
