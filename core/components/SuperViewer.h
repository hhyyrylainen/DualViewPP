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

    ~SuperViewer();
    
    
protected:

    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

    //! \brief Returns true if DisplayImage has finished loading
    bool IsImageReadyToShow() const;

    //! \brief Called from a timer, forces redraws when things happen
    //! \returns False when Gtk should disable the current timer. This happens
    //! when a new timer has been added
    bool _OnTimerCheck();
    
private:

    //! Currently shown image resource
    std::shared_ptr<LoadedImage> DisplayImage;

    //! Currently used timer for _OnTimerCheck in milliseconds
    int CurrentTimer = 1000;
};
}
