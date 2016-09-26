#pragma once

#include "core/resources/Image.h"
#include "core/CacheManager.h"

#include <gtkmm.h>

#include <chrono>

namespace DV{

//! \brief Image viewing widget
//!
//! Manages drawing ImageMagick images with cairo
//! \todo Make mouse disable function also disconnect singal handlers
//! (https://developer.gnome.org/gtkmm-tutorial/stable/sec-disconnecting-signal-handlers.html.en)
class SuperViewer : public Gtk::DrawingArea{

    using ClockType = std::chrono::high_resolution_clock;
    
public:

    
    //! \brief Constructor called by glade builder when loading a widget of this type
    SuperViewer(_GtkDrawingArea* area, Glib::RefPtr<Gtk::Builder> builder,
        std::shared_ptr<Image> displayedResource);

    ~SuperViewer();
    
    
protected:

    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

    //! \brief Draws the CachedDrawnImage with all the current settings
    void _DrawCurrentImage(const Cairo::RefPtr<Cairo::Context>& cr) const;

    //! \brief Returns true if DisplayImage has finished loading
    bool IsImageReadyToShow() const;

    //! \brief Called when switching from a full image to a thumbnail image
    void _SwitchToThumbnailMode(bool isthumbnail);

    //! \brief Resets variables both for _OnNewImageReady and _SwitchToThumbnailMode
    void _ResetImageInstanceVariables();

    //! \brief Called from a timer, forces redraws when things happen
    //! \returns False when Gtk should disable the current timer. This happens
    //! when a new timer has been added
    bool _OnTimerCheck();

    //! \brief Resets all per image variables to start rendering from a fresh state
    void _OnNewImageReady();

    //! \brief Sets the LoadedImage to be shown
    void _SetLoadedImage(std::shared_ptr<LoadedImage> image);

    bool _OnMouseMove(GdkEventMotion* motion_event);

    bool _OnMouseButtonPressed(GdkEventButton* event);
    bool _OnMouseButtonReleased(GdkEventButton* event);

    bool _OnKeyPressed(GdkEventKey* event);

    bool _OnScroll(GdkEventScroll* event);
    
private:

    //! The image to show
    std::shared_ptr<Image> DisplayedResource;
    
    //! Currently shown image resource
    std::shared_ptr<LoadedImage> DisplayImage;

    //! Holds a cached version of DisplayImage. Prevents conversion happening on each frame
    Glib::RefPtr<Gdk::Pixbuf> CachedDrawnImage;

    //! Used to call _OnNewImageReady once
    bool IsImageReady = false;

    //! True when there are multiple frames in DisplayImage
    bool IsMultiFrame = false;

    float ImageZoom = 1.0f;

    //Point BaseOffset = new Point(0, 0);

    bool ResetZoom = true;

    //! Set to true to allow this to react to image change keys / arrow keys
    bool ReactToKeyPress = false;

    bool OriginalIsAutoFit = true;
    bool IsAutoFit = true;

    //! If true currently has the thumbnail image loaded
    bool IsInThumbnailMode = false;


    //! Used to browse images in a collection
    //std::shared_ptr<Collection> ContainedInCollection = nullptr;

    //! Disables zooming with the scroll wheel
    bool DisableMouseScroll = false;

    //! Disables dragging with the mouse. Required to be false if drag events shouldn't
    //! be handled by this
    bool DisableDragging = false;

    //! Disables moving to next/previous image with keys
    bool DisableKeyPresses = false;

    //! If true will only ever load a thumbnail of the image
    bool ForceOnlyThumbnail = false;

    //! Current frame of animation
    size_t CurrentAnimationFrame = 0;

    //! Used to show multi frame images at the right speed
    ClockType::time_point LastFrame;

    //! Currently used timer for _OnTimerCheck in milliseconds
    int CurrentTimer = 1000;


    //! True if mouse is down and dragging can start
    bool CanStartDrag = false;
    
    //! True when dragging the image around
    bool DoingDrag = false;

    //        Point DragStartPos = new Point(0, 0);

    // Point OffsetBeforeDrag = new Point(0, 0);
};
}
