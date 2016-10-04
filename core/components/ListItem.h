#pragma once

#include "SuperViewer.h"
#include "core/resources/Image.h"

#include <gtkmm.h>

#include <memory>


namespace DV{

//! \brief Base class for all widget types that are used with a SuperContainer
class ListItem : public Gtk::Frame{
public:

    ListItem(std::shared_ptr<Image> showImage, const std::string &name);
    ~ListItem();

protected:

    //! \brief Sets text for NameLabel
    void _SetName(const std::string &name);

    //! \brief Sets the image on ImageIcon
    void _SetImage(std::shared_ptr<Image> image);

    // Gtk overrides
    Gtk::SizeRequestMode get_request_mode_vfunc() const override;
    void get_preferred_width_vfunc(int& minimum_width, int& natural_width) const override;
    void get_preferred_height_for_width_vfunc(int width,
        int& minimum_height, int& natural_height) const override;
    void get_preferred_height_vfunc(int& minimum_height, int& natural_height) const override;
    // void get_preferred_width_for_height_vfunc(int height,
    //     int& minimum_width, int& natural_width) const override;



    
    
private:

    Gtk::Box Container;
    
    SuperViewer ImageIcon;

    Gtk::Overlay TextAreaOverlay;
    Gtk::Label NameLabel;

    //! If true will always be the same size
    bool ConstantSize = false;
};
}
