#pragma once

#include "SuperViewer.h"
#include "core/resources/Image.h"

#include <gtkmm.h>


namespace DV{

class ListItem : public Gtk::Box{
public:

    ListItem(std::shared_ptr<Image> showImage, const std::string &name);
    ~ListItem();

protected:
    
    void _SetName(const std::string &name);

    // Gtk overrides
    Gtk::SizeRequestMode get_request_mode_vfunc() const override;
    void get_preferred_width_vfunc(int& minimum_width, int& natural_width) const override;
    void get_preferred_height_for_width_vfunc(int width,
        int& minimum_height, int& natural_height) const override;
    void get_preferred_height_vfunc(int& minimum_height, int& natural_height) const override;
    // void get_preferred_width_for_height_vfunc(int height,
    //     int& minimum_width, int& natural_width) const override;

private:

    SuperViewer ImageIcon;

    Gtk::Overlay TextAreaOverlay;
    Gtk::Label NameLabel;

    //! If true will always be the same size
    bool ConstantSize = false;
};
}
