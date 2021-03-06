#pragma once

#include "BaseWindow.h"
#include "IsAlive.h"

#include "Common/BaseNotifiable.h"

#include <gtkmm.h>

namespace DV {

class SuperContainer;
class Collection;
class TagEditor;
class Image;

//! \brief Window that shows a single Collection
class SingleCollection : public BaseWindow,
                         public Gtk::Window,
                         public IsAlive,
                         public Leviathan::BaseNotifiableAll {
public:
    SingleCollection(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~SingleCollection();

    //! \brief Sets the shown Collection
    void ShowCollection(std::shared_ptr<Collection> collection);

    //! \brief Updates the shown images
    void ReloadImages(Lock& guard);

    //! \brief Sets tag editor visible or hides it
    void ToggleTagEditor();

    void StartRename();

    void Reorder();

    //! \brief Called when an image is added or removed from the collection
    void OnNotified(
        Lock& ownlock, Leviathan::BaseNotifierAll* parent, Lock& parentlock) override;

    //! \brief Returns all selected images
    std::vector<std::shared_ptr<Image>> GetSelected() const;

protected:
    void _OnDeleteSelected();
    void _OnOpenSelectedInImporter();
    void _OnDeleteRestorePressed();

    void _OnClose() override;

    void _UpdateDeletedStatus();
    void _PerformDelete(size_t orphanCount);

private:
    SuperContainer* ImageContainer;

    TagEditor* CollectionTags;
    Gtk::ToolButton* OpenTagEdit;
    Gtk::ToolButton* DeleteSelected;
    Gtk::ToolButton* ReorderThisCollection;
    Gtk::ToolButton* OpenSelectedImporter;
    Gtk::ToolButton* DeleteThisCollection;

    Gtk::Label* StatusLabel;

    std::shared_ptr<Collection> ShownCollection;
};

} // namespace DV
