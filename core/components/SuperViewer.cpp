// ------------------------------------ //
#include "SuperViewer.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //

constexpr auto MAX_LOADING_LINES = 6;

SuperViewer::SuperViewer(_GtkDrawingArea* area, Glib::RefPtr<Gtk::Builder> builder,
    std::shared_ptr<Image> displayedResource) :
    Gtk::DrawingArea(area), DisplayedResource(displayedResource)
{
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

    signal_size_allocate().connect(sigc::mem_fun(*this, &SuperViewer::_OnResize));

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

    // If no image, stop //
    if(!DisplayedResource){

        // paint the background
        auto refStyleContext = get_style_context();
        refStyleContext->render_background(cr,
        allocation.get_x(), allocation.get_y(),
        allocation.get_width(), allocation.get_height());

        _AddRedrawTimer(-1);
        return true;
    }

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
        // Pen pen = new Pen(Brushes.CadetBlue);

        // CadetBlue
        cr->set_source_rgb(0.37, 0.61, 0.63);
        cr->set_line_width(4);

        const auto now = ClockType::now();

        if(now - LastFrame >= std::chrono::milliseconds(100)){

            ++LoadingLineCount;

            if(LoadingLineCount > MAX_LOADING_LINES)
                LoadingLineCount = 0;

            LastFrame = now;
        }

        // Set redraw //
        _AddRedrawTimer(100);

        for(int i = 1; i <= LoadingLineCount; ++i){

            // Going up //
            cr->move_to(width / 2 - 25 * (((float)i / MAX_LOADING_LINES) + 1),
                height / 2 + (10 * i));

            cr->line_to((int)(width / 2 + 25 * (((float)i / MAX_LOADING_LINES) + 1)),
                height / 2 + (10 * i));

            // Going down //
            cr->move_to(width / 2 - 25 * (((float)i / MAX_LOADING_LINES) + 1),
                height / 2 - (10 * i));

            cr->line_to((int)(width / 2 + 25 * (((float)i / MAX_LOADING_LINES) + 1)),
                height / 2 - (10 * i));            
        }
        
        cr->stroke();

    } else {

        if(!IsImageReady){

            // Reset things //
            _OnNewImageReady();
        }

        if(!DisplayImage->IsValid()){

            // Draw error message //
            //const std::string error;
            //DisplayImage->GetError(error);
            
        } else {

            // Move to next frame if multiframe //
            if(IsMultiFrame){

                const auto now = ClockType::now();
                const auto timeSinceFrame = now - LastFrame;

                if(timeSinceFrame + std::chrono::milliseconds(3)
                    >= DisplayImage->GetAnimationTime(CurrentAnimationFrame))
                {
                    // Move to next frame //
                    ++CurrentAnimationFrame;
                    LastFrame = now;

                    // Recreate the frame when rendering //
                    CachedDrawnImage.reset();

                    // Loop
                    if(CurrentAnimationFrame >= DisplayImage->GetFrameCount())
                        CurrentAnimationFrame = 0;
                }

                // Queue redraw for next frame //
                _AddRedrawTimer(std::chrono::duration_cast<std::chrono::milliseconds>(
                    DisplayImage->GetAnimationTime(CurrentAnimationFrame) -
                    (now - LastFrame)).count());
            } else {

                // Disable redraw timer //
                if(CurrentTimer >= 0)
                    _AddRedrawTimer(-1);
            }
        
            // Draw positioned image //
            if(!CachedDrawnImage)
                CachedDrawnImage = DisplayImage->CreateGtkImage(CurrentAnimationFrame);

            _DrawCurrentImage(cr);
        }
    }
    
    return true;
}

void SuperViewer::_DrawCurrentImage(const Cairo::RefPtr<Cairo::Context>& cr) const{

    LEVIATHAN_ASSERT(CachedDrawnImage, "CachedDrawnImage is invalid in draw current");

    // Move to right position //
    const auto topLeft = CalculateImageRenderTopLeft(CachedDrawnImage->get_width(),
        CachedDrawnImage->get_height(), ImageZoom);

    const auto finalOffset = topLeft + BaseOffset;
    
    cr->translate(finalOffset.X, finalOffset.Y);
    
    // Scale the whole drawing area //
    cr->scale(ImageZoom, ImageZoom);

    // Draw to a rectangle normally //
    Gdk::Cairo::set_source_pixbuf(cr, CachedDrawnImage, 0, 0);
    cr->rectangle(0, 0,
        CachedDrawnImage->get_width(), CachedDrawnImage->get_height());
    cr->fill();    
}
// ------------------------------------ //
SuperViewer::Point SuperViewer::CalculateImageRenderTopLeft(size_t width, size_t height,
    float zoomlevel) const
{
    const Point centerpoint(
        (get_width() / 2) + BaseOffset.X,
        (get_height() / 2) + BaseOffset.Y);

    auto theight = static_cast<size_t>(height * zoomlevel);
    auto twidth = static_cast<size_t>(width * zoomlevel);

    return Point(centerpoint.X - (twidth / 2), centerpoint.Y - (theight / 2));
}

void SuperViewer::DoAutoFit(){

    BaseOffset = Point(0, 0);
    ImageZoom = 1.0f;

    float xdifference = get_width() / (float)DisplayImage->GetWidth();

    LEVIATHAN_ASSERT(std::round(DisplayImage->GetWidth() * xdifference) == get_width(),
        "Invalid math assumption");

    float ydifference = get_height() / (float)DisplayImage->GetHeight();

    LEVIATHAN_ASSERT(std::round(DisplayImage->GetHeight() * ydifference) == get_height(),
                    "Invalid math assumption");

    float scale = std::min(xdifference, ydifference);

    if(scale < 1.0f || IsInThumbnailMode)
        ImageZoom = scale;
    
    get_width();
    get_height();
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
    IsMultiFrame = DisplayImage->GetFrameCount() > 1;

    IsMultiFrame = false;
    CurrentAnimationFrame = 0;
    BaseOffset = Point(0, 0);
    DoingDrag = false;
    CachedDrawnImage.reset();

    if(ResetZoom)
        ImageZoom = 1.0f;

    if (IsAutoFit || IsInThumbnailMode)
        DoAutoFit();

    LastFrame = ClockType::now();
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

    // Force redraw //
    queue_draw();
}
// ------------------------------------ //
bool SuperViewer::MoveInCollection(bool forwards, bool wrap /*= true*/){

    return false;

    // if (ContainedInCollection == null || ViewedResource == null)
    //     return;

    // var next = ContainedInCollection.GetNextImage(ViewedResource, direction);

    // if (next != null)
    //     ViewedResource = next;
}

// ------------------------------------ //
void SuperViewer::_AddRedrawTimer(int32_t ms){

    if(CurrentTimer == ms)
        return;

    CurrentTimer = ms;

    if(ms <= 0){

        // No new timer wanted //
        return;
    }

    // Add redraw timer //
    Glib::signal_timeout().connect(sigc::bind<int32_t>(
            sigc::mem_fun(*this, &SuperViewer::_OnTimerCheck), ms),
        CurrentTimer);
}

bool SuperViewer::_OnTimerCheck(int32_t currenttime){

    // If currenttime is no longer CurrentTimer, we have been replaced //
    if(currenttime != CurrentTimer){

        return false;
    }
    
    // Cause a redraw //
    queue_draw();

    return true;
}

bool SuperViewer::_OnMouseMove(GdkEventMotion* motion_event){

    if(DisableDragging || (!CanStartDrag && !DoingDrag))
        return false;

    Point mousePos = Point(motion_event->x, motion_event->y);

    // Check can start //
    if(!DoingDrag && CanStartDrag){

        if((DragStartPos - mousePos).HAddAbs() > 8){

            DoingDrag = true;
            OffsetBeforeDrag = BaseOffset;
        }
    }
    
    if(DoingDrag){

        CanStartDrag = false;

        BaseOffset = OffsetBeforeDrag;

        BaseOffset += (mousePos - DragStartPos);
        
        LOG_WRITE("Drag");
        queue_draw();
    } 

    return true;
}

bool SuperViewer::_OnMouseButtonPressed(GdkEventButton* event){

    if(DisableDragging)
        return false;

    // Double click
    if(event->type == GDK_2BUTTON_PRESS)
        LOG_WRITE("Double click");

    // Left mouse //
    if(event->button == 1){

        CanStartDrag = true;
        DragStartPos = Point(event->x, event->y);
    }

    return false;
}

bool SuperViewer::_OnMouseButtonReleased(GdkEventButton* event){

    if(DisableDragging)
        return false;

    // Left mouse //
    if(event->button == 1){

        if(!DoingDrag){

            // Single click //
            LOG_WRITE("Single click");
        }
        
        DoingDrag = false;
        CanStartDrag = false;
    }

    return false;
}

bool SuperViewer::_OnKeyPressed(GdkEventKey* event){

    if(DisableKeyPresses)
        return false;

    switch(event->keyval){
    case GDK_KEY_Left:
    {
        LOG_WRITE("Left");
        MoveInCollection(false, true);
        return true;
    }
    case GDK_KEY_Right:
    {
        LOG_WRITE("Right");
        MoveInCollection(true, true);
        return true;
    }
    default:
        return false;
    }

    return false;
}

bool SuperViewer::_OnScroll(GdkEventScroll* event){

    if(DisableMouseScroll)
        return false;

    if(!IsImageReady)
        return false;

    if(BaseOffset.X != 0 || BaseOffset.Y != 0)
    {
        Point mousepos = Point(event->x, event->y);

        int theight = (int)(DisplayImage->GetHeight() * ImageZoom);
        int twidth = (int)(DisplayImage->GetWidth() * ImageZoom);

        Point topleft = CalculateImageRenderTopLeft(DisplayImage->GetWidth(),
            DisplayImage->GetHeight(), ImageZoom);

        Point imagesizerelative = Point(
            (mousepos.X - topleft.X) / (float)twidth,
            (mousepos.Y - topleft.Y) / (float)theight);

        ImageZoom *= 1 + (0.001f * event->delta_y);
        
        theight = (int)(DisplayImage->GetHeight() * ImageZoom);
        twidth = (int)(DisplayImage->GetWidth() * ImageZoom);
        
        Point newtopleft = CalculateImageRenderTopLeft(DisplayImage->GetWidth(),
            DisplayImage->GetHeight(), ImageZoom);

        Point newimagesizerelative = Point(
            (mousepos.X - newtopleft.X) / (float)twidth,
            (mousepos.Y - newtopleft.Y) / (float)theight);

        Point difference = Point(imagesizerelative.X - newimagesizerelative.X,
            imagesizerelative.Y - newimagesizerelative.Y);

        Point pixeldifference = Point((int)(difference.X * twidth),
            (int)(difference.Y * theight));

        BaseOffset.X -= pixeldifference.X;
        BaseOffset.Y -= pixeldifference.Y;

        // Verification code //
        Point verifyleft = CalculateImageRenderTopLeft(DisplayImage->GetWidth(),
            DisplayImage->GetHeight(), ImageZoom);

        Point verifyrelative = Point(
            (mousepos.X - verifyleft.X) / (float)twidth,
            (mousepos.Y - verifyleft.Y) / (float)theight);

        LEVIATHAN_ASSERT(std::abs(verifyrelative.X - imagesizerelative.X) < 1.001f,
            "Invalid math assumption");
        LEVIATHAN_ASSERT(std::abs(verifyrelative.Y - imagesizerelative.Y) < 1.001f,
            "Invalid math assumption");
    }
    else
    {
        ImageZoom *= 1 + (0.001f * event->delta_y);
    }

    IsAutoFit = false;
    queue_draw();
    return true;
}

void SuperViewer::_OnResize(Gtk::Allocation &allocation){

    if(!IsImageReady)
        return;

    LOG_INFO("Resized");
    
    if (IsAutoFit || IsInThumbnailMode)
        DoAutoFit();
    
}
