#pragma once

#include "core/resources/Image.h"
#include "core/CacheManager.h"

#include <gtkmm.h>

namespace DV{

//! \brief Image viewing widget
//!
//! Manages drawing ImageMagick images with cairo
class SuperViewer : public Gtk::DrawingArea{
public:

    
    //! \brief Constructor called by glade builder when loading a widget of this type
    SuperViewer(_GtkDrawingArea* area, Glib::RefPtr<Gtk::Builder> builder,
        std::shared_ptr<Image> displayedResource);
    
    
protected:

    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

    //! \brief Returns true if DisplayImage has finished loading
    bool IsImageReadyToShow() const;
    
private:

    //! Currently shown image resource
    std::shared_ptr<LoadedImage> DisplayImage;

    
};
}
