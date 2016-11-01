#pragma once

#include "BaseWindow.h"
#include "core/IsAlive.h"

#include "core/components/SuperViewer.h"

#include <gtkmm.h>

namespace DV{

//! \brief Window that shows a single image
//! \todo Make this use similar constructor as Importer
class SingleView : public BaseWindow, public Gtk::Window, public IsAlive{
public:

    SingleView(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~SingleView();
    
    //! \brief Opens a window with an existing resource
    void Open(std::shared_ptr<Image> image);

    //! \brief Updates the shown tags
    void OnTagsUpdated();
    
protected:

    void _OnClose() override;
    
private:

    SuperViewer* ImageView = nullptr;
    Gtk::Label* TagsLabel = nullptr;
};

}

