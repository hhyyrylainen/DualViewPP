#pragma once

#include "SuperViewer.h"
#include "core/resources/Image.h"

#include <gtkmm.h>

#include <memory>


namespace DV{

//! \brief Base class for all widget types that are used with a SuperContainer
class ListItem : public Gtk::Frame{
public:

    //! \param showimage The image to show
    //! \param name The text to show under the display image
    //! \param selectable If true allows this item to be selected/unselected by clicking
    //! \param allowpopup If true allows this item to open a popup window when double clicked
    ListItem(std::shared_ptr<Image> showimage, const std::string &name,
        const ItemSelectable &selectable, bool allowpopup);
    
    ~ListItem();

    //! \brief Sets selected status. Changes background colour
    void SetSelected(bool selected);

    //! \brief Returns true if this is selected
    inline bool IsSelected() const{

        return CurrentlySelected;
    }

    //! \brief Returns the image shown in ImageIcon
    std::shared_ptr<Image> GetPrimaryImage() const;

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

    bool _OnMouseButtonPressed(GdkEventButton* event);


    //! \brief Called when selection status has been updated
    virtual void _OnSelectionUpdated();

    //! \brief Called when the image part is double clicked. By default does nothing
    virtual void _DoPopup();
    
    
protected:

    Gtk::EventBox Events;
    Gtk::Box Container;
    
    SuperViewer ImageIcon;

    Gtk::Overlay TextAreaOverlay;
    Gtk::Label NameLabel;

    //! If true will always be the same size
    bool ConstantSize = false;

    //! If this is selectable this indicates whether the user has selected this item or not
    bool CurrentlySelected = false;

    //! If false doesn't listen for mouse clicks
    //! When true updates selected state when clicked
    bool Selectable;

    //! If true listens for double click events
    bool AllowPopUpWIndow;

    //! Called from default _OnSelectionUpdated
    std::function<void (ListItem&)> OnSelected;
};
}
