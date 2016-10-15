#pragma once

#include "core/resources/Image.h"
#include "core/CacheManager.h"

#include "Common/Types.h"

#include <gtkmm.h>

#include <chrono>

namespace DV{

//! If the widget size is below this value the viewer will go into thumbnail mode
constexpr auto SUPER_THUMBNAIL_WIDTH_THRESHOLD = 250;
//! If the widget size is below this value the viewer will go into thumbnail mode
constexpr auto SUPER_THUMBNAIL_HEIGHT_THRESHOLD = 225;

//! The amount of time in milliseconds that has elapsed since last draw after
//! the cached image is released
constexpr auto SUPER_UNLOAD_IMAGE_AFTER_MS = 15000;


//! \brief Image viewing widget
//!
//! Manages drawing ImageMagick images with cairo
class SuperViewer : public Gtk::DrawingArea{

    using ClockType = std::chrono::high_resolution_clock;
    using Point = Leviathan::Float2;

public:

    enum class ENABLED_EVENTS : uint8_t {

        NONE = 0,
        DRAG = 0x1,
        SCROLL = 0x2,
        POPUP = 0x4,
        MOVE_KEYS = 0x8,
        ALL = DRAG | SCROLL | POPUP | MOVE_KEYS
    };
    
public:

    //! \brief Non-glade constructor
    SuperViewer(std::shared_ptr<Image> displayedResource, ENABLED_EVENTS events,
        bool forcethumbnail);
    
    //! \brief Constructor called by glade builder when loading a widget of this type
    SuperViewer(_GtkDrawingArea* area, Glib::RefPtr<Gtk::Builder> builder,
        std::shared_ptr<Image> displayedResource, bool forcethumbnail);

    ~SuperViewer();

    //! \brief Moves between collection images
    //!
    //! Only works if this object is told about the current collection
    //! \param forwards If true moves forwards, if false moves backwards
    //! \param wrap If true will continue from the first image if the end is reached
    bool MoveInCollection(bool forwards, bool wrap = true);

    //! \brief Sets the image to show
    void SetImage(std::shared_ptr<Image> displayedResource);

    //! \brief Gets the shown image
    inline auto GetImage() const{

        return DisplayedResource;
    }

    //! \brief Opens the current image in a new window
    void OpenImageInNewWindow();
    
    
protected:

    //! \brief Common constructor code for all constructor overloads
    void _CommonCtor();
    
    //! \brief Main drawing function
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

    //! \brief Used to unload the current image if this hasn't been drawn for a few seconds
    bool _OnUnloadTimer();

    //! \brief Starts an unload timer, should be called whenever CachedDrawnImage is set
    void _AddUnloadTimer();

    //! \brief Unloads the image immediately
    void _OnUnMapped();

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

    ENABLED_EVENTS Events;

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

    //! Used to unload CachedDrawnImage after some time of not rendering
    bool HasBeenDrawn = false;

    //! If true has a unload timer
    bool HasUnloadTimer = false;
};
}
