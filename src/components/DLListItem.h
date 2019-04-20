#pragma once

#include "IsAlive.h"

#include "Common/BaseNotifiable.h"

#include <gtkmm.h>

namespace DV {

class NetGallery;

//! \brief Holds things about a collection of images that's ready to be downloaded
class DLListItem : public Gtk::Frame, public IsAlive, public Leviathan::BaseNotifiableAll {
public:
    DLListItem(std::shared_ptr<NetGallery> todownload);
    ~DLListItem();

    void SetRemoveCallback(std::function<void(DLListItem&)> callback);

    //! \brief Sets the current progress. Valid range: 0.0f - 1.0f
    void SetProgress(float value);

    //! \brief Reads properties from Gallery and updates widgets
    //! \note This will always be run on the main thread when it's free
    void ReadGalleryData();

    auto GetGallery() const
    {
        return Gallery;
    }

    bool IsSelected() const;

    //! \brief Sets this selected
    void SetSelected();

    //! \brief Prevents the user from changing the selected switch
    void LockSelected(bool locked);

    //! \brief Called when the gallery changes properties
    void OnNotified(
        Lock& ownlock, Leviathan::BaseNotifierAll* parent, Lock& parentlock) override;

protected:
    //! \brief Updates the gallery name
    void OnNameUpdated();

    void OnPressedRemove();

protected:
    //! The gallery that is being edited / progress shown on
    std::shared_ptr<NetGallery> Gallery;

    std::function<void(DLListItem&)> OnRemoveCallback;

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

} // namespace DV
