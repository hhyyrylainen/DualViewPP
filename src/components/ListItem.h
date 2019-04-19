#pragma once

#include "IsAlive.h"
#include "SuperViewer.h"
#include "resources/Image.h"

#include <gtkmm.h>

#include <memory>


namespace DV {

//! \brief Base class for all widget types that are used with a SuperContainer
class ListItem : public Gtk::Frame, public IsAlive {
public:
    //! \param showimage The image to show
    //! \param name The text to show under the display image
    //! \param selectable If non-null allows this item to be selected/unselected by clicking
    //! \param allowpopup If true allows this item to open a popup window when double clicked
    //! \note allowpopup Cannot be used at the same time with
    //! DV::ItemSelectable::UsesCustomPopup because the default double click handler will
    //! eat the event
    ListItem(std::shared_ptr<Image> showimage, const std::string& name,
        const std::shared_ptr<ItemSelectable>& selectable, bool allowpopup);

    ~ListItem();

    //! \brief Sets selected status. Changes background colour
    void SetSelected(bool selected);

    //! \brief Deselects this if currently selected and selecting is enabled
    inline void Deselect()
    {
        if(Selectable && Selectable->Selectable && CurrentlySelected)
            SetSelected(false);
    }

    //! \brief Selects this isn't currently selected and selecting is enabled
    inline void Select()
    {
        if(Selectable && Selectable->Selectable && !CurrentlySelected)
            SetSelected(true);
    }

    //! \brief Returns true if this is selected
    inline bool IsSelected() const
    {
        return CurrentlySelected;
    }

    //! \brief Sets active status. Changes background colour and disables selecting
    void SetActive(bool active);

    inline void Deactivate()
    {
        if(Active)
            SetActive(false);
    }

    inline void Activate()
    {
        if(!Active)
            SetActive(true);
    }

    inline bool IsActive() const
    {
        return Active;
    }

    //! \brief Sets new size.
    //! \note The parent container needs to call this or be otherwise notified
    //! that this has changed, otherwise the size won't actually change.
    //! This is virtual so that FolderListItem can change to non-homogeneous layout
    virtual void SetItemSize(LIST_ITEM_SIZE newsize);

    //! \brief Returns the image shown in ImageIcon
    std::shared_ptr<Image> GetPrimaryImage() const;

    //! \brief Returns the default label text
    auto GetName() const
    {
        return NameLabel.get_text();
    }

protected:
    //! \brief Sets text for NameLabel
    void _SetName(const std::string& name);

    //! \brief Sets the image on ImageIcon
    void _SetImage(std::shared_ptr<Image> image);

    // Gtk overrides
    Gtk::SizeRequestMode get_request_mode_vfunc() const override;
    void get_preferred_width_vfunc(int& minimum_width, int& natural_width) const override;
    void get_preferred_height_for_width_vfunc(
        int width, int& minimum_height, int& natural_height) const override;
    void get_preferred_height_vfunc(int& minimum_height, int& natural_height) const override;
    // void get_preferred_width_for_height_vfunc(int height,
    //     int& minimum_width, int& natural_width) const override;

    bool _OnMouseButtonPressed(GdkEventButton* event);


    //! \brief Called when selection status has been updated
    virtual void _OnSelectionUpdated();

    //! \brief Called when the image part is double clicked. By default does nothing
    virtual void _DoPopup();

    //! \brief Opens context menu if there is one
    //! \returns True if handled
    virtual bool _OnRightClick(GdkEventButton* causedbyevent);

    //! \brief Called when this item is either made inactive or active
    virtual void _OnInactiveStatusUpdated();


protected:
    Gtk::EventBox Events;
    Gtk::Box Container;

    SuperViewer ImageIcon;

    Gtk::Overlay TextAreaOverlay;
    Gtk::Label NameLabel;

    //! If false then this is inactive and won't respond to selection events, but does still
    //! allow popups
    bool Active = true;

    //! If this is selectable this indicates whether the user has selected this item or not
    bool CurrentlySelected = false;

    //! The size of this item
    LIST_ITEM_SIZE ItemSize = LIST_ITEM_SIZE::NORMAL;

    //! If false doesn't listen for mouse clicks
    //! When true updates selected state when clicked
    std::shared_ptr<ItemSelectable> Selectable = nullptr;
};
} // namespace DV
