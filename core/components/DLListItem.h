#pragma once

#include "core/IsAlive.h"

#include <gtkmm.h>

namespace DV{

class NetGallery;

//! \brief Holds things about a collection of images that's ready to be downloaded
class DLListItem : public Gtk::Frame, public IsAlive{
public:

    DLListItem(std::shared_ptr<NetGallery> todownload);
    ~DLListItem();


    //! \brief Sets the current progress. Valid range: 0.0f - 1.0f
    void SetProgress(float value);
    
    //! \brief Reads properties from Gallery and updates widgets
    void ReadGalleryData();

    auto GetGallery() const{

        return Gallery;
    }

protected:


protected:

    //! The gallery that is being edited / progress shown on
    std::shared_ptr<NetGallery> Gallery;

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
