// ------------------------------------ //
#include "SuperViewer.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //

SuperViewer::SuperViewer(_GtkDrawingArea* area, Glib::RefPtr<Gtk::Builder> builder,
    std::shared_ptr<Image> displayedResource) :
    Gtk::DrawingArea(area), DisplayedResource(displayedResource)
{
    // Add redraw timer //
    Glib::signal_timeout().connect(sigc::mem_fun(*this, &SuperViewer::_OnTimerCheck),
        CurrentTimer);

    // Register for events //
    add_events(
        Gdk::POINTER_MOTION_MASK |
        Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK |
        Gdk::SCROLL_MASK |
        Gdk::KEY_PRESS_MASK);

    signal_motion_notify_event().connect(sigc::mem_fun(*this, &SuperViewer::_OnMouseMove));

    signal_button_press_event().connect(
        sigc::mem_fun(*this, &SuperViewer::_OnMouseButtonPressed));
    signal_button_release_event().connect(
        sigc::mem_fun(*this, &SuperViewer::_OnMouseButtonReleased));
    
    signal_scroll_event().connect(sigc::mem_fun(*this, &SuperViewer::_OnScroll));
    signal_key_press_event().connect(sigc::mem_fun(*this, &SuperViewer::_OnKeyPressed));

    // Do setup stuff //
}

SuperViewer::~SuperViewer(){

    LOG_WRITE("Super destructed");

    
}
// ------------------------------------ //
bool SuperViewer::on_draw(const Cairo::RefPtr<Cairo::Context>& cr){

    Gtk::Allocation allocation = get_allocation();
    const int width = allocation.get_width();
    const int height = allocation.get_height();

    // Verify thumbnail mode //
    const bool shouldBeThumbnailMode = ForceOnlyThumbnail || (width <= 256 && height <= 256);
    if(shouldBeThumbnailMode != IsInThumbnailMode){

        _SwitchToThumbnailMode(shouldBeThumbnailMode);
    }

    // If there is no image here, load the full image //
    if(!DisplayImage){

        _SetLoadedImage(DisplayedResource->GetImage());
    }

    // paint the background
    //auto refStyleContext = get_style_context();
    //refStyleContext->render_background(cr,
    //allocation.get_x(), allocation.get_y(),
    //allocation.get_width(), allocation.get_height());

    //angle += (Leviathan::PI * 2) / 360;

    // cr->set_source_rgb(0.8, 0.0, 0.0);
    
    // cr->move_to(center.X, center.Y);
    // cr->line_to(center.X + (dir.X * 50), center.Y + (dir.Y * 50));


    // cr->move_to(center.X, center.Y);
    // cr->line_to(center.X - (dir.X * 50), center.Y - (dir.Y * 50));

    // cr->stroke();

    if(!IsImageReadyToShow()){

        // Draw loading animation //
        
    } else {

        if(!IsImageReady){

            // Reset things //
            _OnNewImageReady();
        }

        if(!DisplayImage->IsValid()){

            // Draw error message //
            
            
        } else {
        
            // Draw positioned image //

            if(!CachedDrawnImage)
                CachedDrawnImage = DisplayImage->CreateGtkImage();

            _DrawCurrentImage(cr);
        }
    }
    
    return true;
}

void SuperViewer::_DrawCurrentImage(const Cairo::RefPtr<Cairo::Context>& cr) const{

    LEVIATHAN_ASSERT(CachedDrawnImage, "CachedDrawnImage is invalid in draw current");

    // Move to right position //
    
    
    // Scale the whole drawing area //
    cr->scale(ImageZoom, ImageZoom);

    // Draw to a rectangle normally //
    Gdk::Cairo::set_source_pixbuf(cr, CachedDrawnImage, 0, 0);
    cr->rectangle(0, 0,
        CachedDrawnImage->get_width(), CachedDrawnImage->get_height());
    cr->fill();    
}
// ------------------------------------ //
bool SuperViewer::IsImageReadyToShow() const{

    if(!DisplayImage)
        return false;

    // If image has been drawn already we can skip checks in DisplayImage
    if(IsImageReady)
        return true;

    return DisplayImage->IsLoaded();
}

void SuperViewer::_OnNewImageReady(){

    LEVIATHAN_ASSERT(DisplayImage, "DisplayImage is invalid in _OnNewImageReady");

    IsImageReady = true;

    // Reset variables //
    _ResetImageInstanceVariables();
    
    IsMultiFrame = DisplayImage->GetFrameCount() > 1;

    LastFrame = ClockType::now();
}

void SuperViewer::_ResetImageInstanceVariables(){
    
    IsMultiFrame = false;
    CurrentAnimationFrame = 0;
    CachedDrawnImage.reset();
}

void SuperViewer::_SwitchToThumbnailMode(bool isthumbnail){

    if(IsInThumbnailMode == isthumbnail)
        return;

    IsInThumbnailMode = isthumbnail;

    _SetLoadedImage(DisplayedResource->GetThumbnail());
}

void SuperViewer::_SetLoadedImage(std::shared_ptr<LoadedImage> image){

    DisplayImage = image;
    IsImageReady = false;
}
// ------------------------------------ //
bool SuperViewer::_OnTimerCheck(){

    int newTimer = 1000;
    
    // Cause a redraw //
    queue_draw();

    if(newTimer != CurrentTimer){

        // Change timer //
        CurrentTimer = newTimer;
        
        Glib::signal_timeout().connect(sigc::mem_fun(*this, &SuperViewer::_OnTimerCheck),
            CurrentTimer);
        
        return false;
    }

    return true;
}

bool SuperViewer::_OnMouseMove(GdkEventMotion* motion_event){

    if(DisableDragging || (!CanStartDrag && !DoingDrag))
        return false;

    LOG_WRITE("Drag");
    queue_draw();

    return true;
}

bool SuperViewer::_OnMouseButtonPressed(GdkEventButton* event){

    if(DisableDragging)
        return false;

    // Double click
    if(event->type == GDK_2BUTTON_PRESS)
        LOG_WRITE("Double click");

    // Left mouse //
    if(event->button == 1)
        CanStartDrag = true;

    return false;
}

bool SuperViewer::_OnMouseButtonReleased(GdkEventButton* event){

    if(DisableDragging)
        return false;

    // Left mouse //
    if(event->button == 1)
        CanStartDrag = false;

    return false;
}

bool SuperViewer::_OnKeyPressed(GdkEventKey* event){

    if(DisableKeyPresses)
        return false;

    LOG_WRITE("Key");

    return false;
}

bool SuperViewer::_OnScroll(GdkEventScroll* event){

    if(DisableMouseScroll)
        return false;

    LOG_WRITE("Scroll");
    
    return true;
}
