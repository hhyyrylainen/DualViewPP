#pragma once

#include "BaseWindow.h"

#include "components/SuperViewer.h"

#include <gtkmm.h>

namespace DV{

//! \brief Window that shows a single image
class SingleView : public BaseWindow{
public:

    //! \brief Opens this window with a file
    //! \exception Leviathan::InvalidArgument if file is not a supported type
    SingleView(const std::string &file);
    ~SingleView();
    
    //! \brief Closes this window if not already closed
    void Close() override;
    
private:

    bool _OnClosed(GdkEventAny* event);
    
private:

    Glib::RefPtr<Gtk::Builder> Builder;

    Gtk::Window* OurWindow = nullptr;
    SuperViewer* ImageView = nullptr;
};

}

