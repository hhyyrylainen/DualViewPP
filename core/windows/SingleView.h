#pragma once

#include "BaseWindow.h"

#include "core/components/SuperViewer.h"

#include <gtkmm.h>

namespace DV{

//! \brief Window that shows a single image
//! \todo Make this use similar constructor as Importer
class SingleView : public BaseWindow{
public:

    //! \brief Opens this window with a file
    //! \exception Leviathan::InvalidArgument if file is not a supported type
    SingleView(const std::string &file);

    //! \brief Opens a window with an existing resource
    SingleView(std::shared_ptr<Image> image);
    
    ~SingleView();
    
private:

    void _CreateWindow(std::shared_ptr<Image> image);

    bool _OnClosed(GdkEventAny* event);

    void _OnClose() override;
    
private:

    Glib::RefPtr<Gtk::Builder> Builder;

    Gtk::Window* OurWindow = nullptr;
    SuperViewer* ImageView = nullptr;
};

}

