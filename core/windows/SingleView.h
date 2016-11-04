#pragma once

#include "BaseWindow.h"
#include "core/IsAlive.h"

#include "leviathan/Common/BaseNotifiable.h"

#include <gtkmm.h>

namespace DV{

class SuperViewer;
class TagEditor;
class Image;

//! \brief Window that shows a single image
//! \note Whenever this receives an event OnTagsUpdated is called
class SingleView : public BaseWindow, public Gtk::Window, public IsAlive,
                     public Leviathan::BaseNotifiableAll
{
public:

    SingleView(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~SingleView();
    
    //! \brief Opens a window with an existing resource
    void Open(std::shared_ptr<Image> image);

    //! \brief Updates the shown tags
    void OnTagsUpdated(Lock &guard);

    //! \brief Sets tag editor visible or hides it
    void ToggleTagEditor();

    //! \brief Called when the shown image changes properties
    void OnNotified(Lock &ownlock, Leviathan::BaseNotifierAll* parent, Lock &parentlock)
        override;
    
protected:

    void _OnClose() override;
    
private:

    SuperViewer* ImageView;

    TagEditor* ImageTags;
    
    Gtk::Label* TagsLabel;
    Gtk::Label* ImageSize;

    // Toolbar
    Gtk::ToolButton EditTagsButton;
};

}

