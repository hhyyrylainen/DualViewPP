#pragma once

#include "BaseWindow.h"

#include "core/components/SuperViewer.h"

#include <gtkmm.h>

namespace DV{

//! \brief Window that shows a single image
class SingleView : public BaseWindow{
public:

    //! \brief Opens this window with a file
    //! \exception Leviathan::InvalidArgument if file is not a supported type
    SingleView(const std::string &file);
    ~SingleView();
    
private:

    bool _OnClosed(GdkEventAny* event);

    void _OnClose() override;
    
private:

    Glib::RefPtr<Gtk::Builder> Builder;

    Gtk::Window* OurWindow = nullptr;
    SuperViewer* ImageView = nullptr;
};

}

