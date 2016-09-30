#pragma once

#include "core/resources/Image.h"
#include "core/CacheManager.h"

#include "Common/Types.h"

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
    using Point = Leviathan::Float2;
    
public:

    
    //! \brief Constructor called by glade builder when loading a widget of this type
    SuperViewer(_GtkDrawingArea* area, Glib::RefPtr<Gtk::Builder> builder,
        std::shared_ptr<Image> displayedResource);

    ~SuperViewer();

    //! \brief Moves between collection images
    //!
    //! Only works if this object is told about the current collection
    //! \param forwards If true moves forwards, if false moves backwards
    //! \param wrap If true will continue from the first image if the end is reached
    bool MoveInCollection(bool forwards, bool wrap = true);
    
    
protected:

    //! \brief Common constructor code for all constructor overloads
    void _CommonCtor(bool hookmouseevents, bool hookkeypressevents);
    
    //! \brief Main drawing function
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

    //! \brief Draws the CachedDrawnImage with all the current settings
    void _DrawCurrentImage(const Cairo::RefPtr<Cairo::Context>& cr) const;

    //! \brief Returns the top left of the image at zoomlevel
    Point CalculateImageRenderTopLeft(size_t width, size_t height,
        float zoomlevel) const;

    //! \brief Automatically adjusts ImageZoom to make the entire image fit
    void DoAutoFit();

    //! \brief Adds a redraw timer with the specific amount of milliseconds
    void _AddRedrawTimer(int32_t ms);


    //! \brief Returns true if DisplayImage has finished loading
    bool IsImageReadyToShow() const;

    //! \brief Called from a timer, forces redraws when things happen
    //! \returns False when Gtk should disable the current timer. This happens
    //! when a new timer has been added
    bool _OnTimerCheck(int32_t currenttime);

    //! \brief Resets all per image variables to start rendering from a fresh state
    void _OnNewImageReady();

    //! \brief Sets the LoadedImage to be shown
    void _SetLoadedImage(std::shared_ptr<LoadedImage> image);
    

    // Gtk events //
    bool _OnMouseMove(GdkEventMotion* motion_event);

    bool _OnMouseButtonPressed(GdkEventButton* event);
    bool _OnMouseButtonReleased(GdkEventButton* event);

    bool _OnKeyPressed(GdkEventKey* event);

    //! \todo Clean and comment this method. Right now it's a mess copied from c# code
    bool _OnScroll(GdkEventScroll* event);

    void _OnResize(Gtk::Allocation &allocation);
    
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

    //! Image size multiplier
    float ImageZoom = 1.0f;

    //! Image offset 
    Point BaseOffset = Point(0, 0);

    //! If true zoom is reset when changing images
    bool ResetZoom = true;

    //! Set to true to allow this to react to image change keys / arrow keys
    bool ReactToKeyPress = false;

    //! \todo find out what this is for. Maybe for setting autofit = true when changing images
    bool OriginalIsAutoFit = true;
    
    //! If true the image will automatically fit to the widget size
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
    int32_t CurrentTimer = 1000;


    //! True if mouse is down and dragging can start
    bool CanStartDrag = false;
    
    //! True when dragging the image around
    bool DoingDrag = false;

    //! Used for mouse drag
    Point DragStartPos = Point(0, 0);
    //! \ditto
    Point OffsetBeforeDrag;

    //! Used for loading animation
    int LoadingLineCount = 1;
};
}
