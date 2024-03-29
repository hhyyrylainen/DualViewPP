// ------------------------------------ //
#include "SuperViewer.h"

#include <utility>

#include "components/ImageListScroll.h"

#include "Common.h"
#include "DualView.h"

using namespace DV;

// ------------------------------------ //
constexpr auto MAX_LOADING_LINES = 6;

SuperViewer::SuperViewer(_GtkDrawingArea* area, Glib::RefPtr<Gtk::Builder> builder,
    std::shared_ptr<Image> displayedResource, ENABLED_EVENTS events, bool forceThumbnail) :
    Gtk::DrawingArea(area),
    DisplayedResource(std::move(displayedResource)), Events(events), ForceOnlyThumbnail(forceThumbnail)
{
    // Do setup stuff //
    _CommonCtor();
}

SuperViewer::SuperViewer(std::shared_ptr<Image> displayedResource, ENABLED_EVENTS events, bool forceThumbnail) :
    DisplayedResource(std::move(displayedResource)), Events(events), ForceOnlyThumbnail(forceThumbnail)
{
    _CommonCtor();
}

void SuperViewer::_CommonCtor()
{
    // Event registration //
    if ((int)Events & (int)ENABLED_EVENTS::DRAG || (int)Events & (int)ENABLED_EVENTS::POPUP)
    {
        // The release and motion events needs to be able to propagate to a containing widget
        // so when enabling one of these all of them must be enabled
        add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::POINTER_MOTION_MASK);

        signal_button_press_event().connect(sigc::mem_fun(*this, &SuperViewer::_OnMouseButtonPressed));
        signal_button_release_event().connect(sigc::mem_fun(*this, &SuperViewer::_OnMouseButtonReleased));
        signal_motion_notify_event().connect(sigc::mem_fun(*this, &SuperViewer::_OnMouseMove));
    }

    // TODO: check do also these event need to enable everything
    if ((int)Events & (int)ENABLED_EVENTS::SCROLL)
    {
        add_events(Gdk::SCROLL_MASK);

        signal_scroll_event().connect(sigc::mem_fun(*this, &SuperViewer::_OnScroll));
    }

    if ((int)Events & (int)ENABLED_EVENTS::MOVE_KEYS)
    {
        add_events(Gdk::KEY_PRESS_MASK);

        set_can_focus(true);

        signal_key_press_event().connect(sigc::mem_fun(*this, &SuperViewer::_OnKeyPressed));
    }

    signal_size_allocate().connect(sigc::mem_fun(*this, &SuperViewer::_OnResize));
    signal_unmap().connect(sigc::mem_fun(*this, &SuperViewer::_OnUnMapped));
}

SuperViewer::~SuperViewer()
{
    DisplayedResource.reset();
}

// ------------------------------------ //
void SuperViewer::SetImage(std::shared_ptr<Image> displayedResource, bool fastUnload /*= false*/)
{
    DisplayedResource = std::move(displayedResource);

    // Reset things //
    DisplayImage.reset();
    CachedDrawnImage.reset();
    IsImageReady = false;
    BumpImageLoadTimer = 0;

    // And reset display settings //
    IsAutoFit = true;

    if (fastUnload)
    {
        // Redraw immediately if wanted to have a previous image go away faster
        queue_draw();
    }

    // Redraw a bit later to give the image time to load
    _AddRedrawTimer(100);

    if (ImageChangeCallback)
        ImageChangeCallback();
}

void SuperViewer::SetBackground(Glib::RefPtr<Gdk::Pixbuf> background)
{
    Background = std::move(background);

    queue_draw();
}

void SuperViewer::SetImage(std::shared_ptr<LoadedImage> alreadyloaded)
{
    DisplayedResource = nullptr;

    DisplayImage = std::move(alreadyloaded);
    IsInThumbnailMode = true;
    ForceOnlyThumbnail = true;
    CachedDrawnImage.reset();

    IsImageReady = false;
    BumpImageLoadTimer = 0;

    IsAutoFit = true;

    queue_draw();

    if (ImageChangeCallback)
        ImageChangeCallback();
}

void SuperViewer::RemoveImage()
{
    DisplayedResource = nullptr;

    // Reset things //
    DisplayImage.reset();
    CachedDrawnImage.reset();
    IsImageReady = false;

    queue_draw();

    if (ImageChangeCallback)
        ImageChangeCallback();
}

// ------------------------------------ //
bool SuperViewer::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    Gtk::Allocation allocation = get_allocation();
    const int width = allocation.get_width();
    const int height = allocation.get_height();

    HasBeenDrawn = true;

    // If no image, stop //
    if (!DisplayedResource && !DisplayImage)
    {
        // paint the background
        auto refStyleContext = get_style_context();
        refStyleContext->render_background(
            cr, allocation.get_x(), allocation.get_y(), allocation.get_width(), allocation.get_height());

        // Draw a grey rectangle //
        cr->set_source_rgb(0.47, 0.47, 0.47);
        cr->rectangle(0, 0, width, height);
        cr->fill();

        _AddRedrawTimer(-1);
        return true;
    }

    // Verify thumbnail mode //
    const bool shouldBeThumbnailMode =
        ForceOnlyThumbnail || (width <= SUPER_THUMBNAIL_WIDTH_THRESHOLD && height <= SUPER_THUMBNAIL_HEIGHT_THRESHOLD);

    if (shouldBeThumbnailMode != IsInThumbnailMode)
    {
        IsInThumbnailMode = shouldBeThumbnailMode;
        _SetLoadedImage(nullptr);
    }

    // If there is no image here, try to load the right image //
    if (!DisplayImage)
    {
        _SetLoadedImage(IsInThumbnailMode ? DisplayedResource->GetThumbnail() : DisplayedResource->GetImage());
    }

    if (!IsImageReadyToShow())
    {
        // Draw loading animation //
        // CadetBlue
        cr->set_source_rgb(0.37, 0.61, 0.63);
        cr->set_line_width(4);

        const auto now = ClockType::now();

        if (now - LastFrame >= std::chrono::milliseconds(100))
        {
            ++LoadingLineCount;

            if (LoadingLineCount > MAX_LOADING_LINES)
                LoadingLineCount = 0;

            if (UseImageLoadPriorityBump)
            {
                BumpImageLoadTimer += 100;

                if (BumpImageLoadTimer >= VIEWER_LOAD_BUMP_INTERVAL)
                {
                    BumpImageLoadTimer = 0;

                    if (DisplayImage)
                        DisplayImage->BumpLoadPriority();
                }
            }

            LastFrame = now;
        }

        // Set redraw //
        _AddRedrawTimer(100);

        for (int i = 1; i <= LoadingLineCount; ++i)
        {
            // Going up //
            cr->move_to(width / 2 - 25 * (((float)i / MAX_LOADING_LINES) + 1), height / 2 + (10 * i));

            cr->line_to((int)(width / 2 + 25 * (((float)i / MAX_LOADING_LINES) + 1)), height / 2 + (10 * i));

            // Going down //
            cr->move_to(width / 2 - 25 * (((float)i / MAX_LOADING_LINES) + 1), height / 2 - (10 * i));

            cr->line_to((int)(width / 2 + 25 * (((float)i / MAX_LOADING_LINES) + 1)), height / 2 - (10 * i));
        }

        cr->stroke();
    }
    else
    {
        if (!IsImageReady)
        {
            // Reset things //
            _OnNewImageReady();
        }

        if (!DisplayImage->IsValid())
        {
            // Draw error message //
            const std::string error = DisplayImage->GetError();

            Pango::FontDescription font;

            font.set_family("Sans 12");

            auto layout = create_pango_layout(error);

            layout->set_font_description(font);
            layout->set_height(height * Pango::SCALE);
            layout->set_width(width * Pango::SCALE);
            layout->set_wrap(Pango::WrapMode::WRAP_WORD);
            layout->set_ellipsize(Pango::EllipsizeMode::ELLIPSIZE_END);

            int textWidth;
            int textHeight;

            // Calculate text dimensions
            layout->get_pixel_size(textWidth, textHeight);

            // Make it be in the center
            cr->move_to((width / 2) - (textWidth / 2), (height / 2) - (textHeight / 2));

            // Set colour
            cr->set_source_rgb(1.0, 0.2, 0.5);

            layout->show_in_cairo_context(cr);
        }
        else
        {
            // Move to next frame if multiframe //
            if (IsMultiFrame)
            {
                const auto now = ClockType::now();
                const auto timeSinceFrame = now - LastFrame;

                if (timeSinceFrame + std::chrono::milliseconds(3) >=
                    DisplayImage->GetAnimationTime(CurrentAnimationFrame))
                {
                    // Move to next frame //
                    ++CurrentAnimationFrame;
                    LastFrame = now;

                    // Recreate the frame when rendering //
                    CachedDrawnImage.reset();

                    // Loop
                    if (CurrentAnimationFrame >= DisplayImage->GetFrameCount())
                        CurrentAnimationFrame = 0;
                }

                // Queue redraw for next frame //
                _AddRedrawTimer(std::chrono::duration_cast<std::chrono::milliseconds>(
                    DisplayImage->GetAnimationTime(CurrentAnimationFrame) - (now - LastFrame))
                                    .count());
            }
            else
            {
                // Disable redraw timer //
                if (CurrentTimer >= 0)
                {
                    _AddRedrawTimer(-1);

                    // Draw twice to avoid weird scrolling bug //
                    queue_draw();
                }
            }

            // Draw positioned image //
            if (!CachedDrawnImage)
            {
                CachedDrawnImage = DisplayImage->CreateGtkImage(CurrentAnimationFrame);

                if (CachedDrawnImage)
                    _AddUnloadTimer();
            }

            // Draw the background first so the other image is on top
            if (Background)
                _DrawBackground(cr);

            _DrawCurrentImage(cr);
        }
    }

    return true;
}

void SuperViewer::_DrawCurrentImage(const Cairo::RefPtr<Cairo::Context>& cr) const
{
    LEVIATHAN_ASSERT(CachedDrawnImage, "CachedDrawnImage is invalid in draw current");

    // Move to right position //
    const auto topLeft =
        CalculateImageRenderTopLeft(CachedDrawnImage->get_width(), CachedDrawnImage->get_height(), ImageZoom);

    const auto finalOffset = topLeft /*+ BaseOffset*/ /* + Point(50, 50)*/;

    cr->translate(finalOffset.X, finalOffset.Y);

    // Scale the whole drawing area //
    cr->scale(ImageZoom, ImageZoom);

    // Draw to a rectangle normally //
    Gdk::Cairo::set_source_pixbuf(cr, CachedDrawnImage, 0, 0);
    cr->rectangle(0, 0, CachedDrawnImage->get_width(), CachedDrawnImage->get_height());
    cr->fill();
}

void SuperViewer::_DrawBackground(const Cairo::RefPtr<Cairo::Context>& cr) const
{
    if (!Background)
        return;

    float xdifference = get_width() / (float)Background->get_width();
    float ydifference = get_height() / (float)Background->get_height();

    float scale = std::min(xdifference, ydifference);

    const Point centerpoint((get_width() / 2), (get_height() / 2));

    auto theight = static_cast<size_t>(Background->get_height() * scale);
    auto twidth = static_cast<size_t>(Background->get_width() * scale);

    // Save the cairo state otherwise we would mess up the normal drawing
    cr->save();

    cr->translate(centerpoint.X - (twidth / 2), centerpoint.X - (twidth / 2));

    // Scale the whole drawing area //
    cr->scale(scale, scale);

    // Draw to a rectangle normally //
    Gdk::Cairo::set_source_pixbuf(cr, Background, 0, 0);
    cr->rectangle(0, 0, Background->get_width(), Background->get_height());
    cr->fill();

    cr->restore();
}

// ------------------------------------ //
SuperViewer::Point SuperViewer::CalculateImageRenderTopLeft(size_t width, size_t height, float zoomlevel) const
{
    const Point centerpoint((get_width() / 2) + BaseOffset.X, (get_height() / 2) + BaseOffset.Y);

    auto theight = static_cast<size_t>(height * zoomlevel);
    auto twidth = static_cast<size_t>(width * zoomlevel);

    return Point(centerpoint.X - (twidth / 2), centerpoint.Y - (theight / 2));
}

void SuperViewer::DoAutoFit()
{
    if (!DisplayImage || !DisplayImage->IsImageObjectLoaded())
        return;

    BaseOffset = Point(0, 0);
    ImageZoom = 1.0f;

    float xdifference = get_width() / (float)DisplayImage->GetWidth();

    LEVIATHAN_ASSERT(std::round(DisplayImage->GetWidth() * xdifference) == get_width(), "Invalid math assumption");

    float ydifference = get_height() / (float)DisplayImage->GetHeight();

    LEVIATHAN_ASSERT(std::round(DisplayImage->GetHeight() * ydifference) == get_height(), "Invalid math assumption");

    float scale = std::min(xdifference, ydifference);

    if (scale < 1.0f || IsInThumbnailMode)
        ImageZoom = scale;
}

// ------------------------------------ //
bool SuperViewer::IsImageReadyToShow() const
{
    if (!DisplayImage)
        return false;

    // If image has been drawn already we can skip checks in DisplayImage
    if (IsImageReady)
        return true;

    return DisplayImage->IsLoaded();
}

void SuperViewer::_OnNewImageReady()
{
    LEVIATHAN_ASSERT(DisplayImage, "DisplayImage is invalid in _OnNewImageReady");

    IsImageReady = true;

    // Check error //
    if (!DisplayImage->IsValid())
    {
        // Set error //

        return;
    }

    // Reset variables //
    IsMultiFrame = DisplayImage->GetFrameCount() > 1;

    CurrentAnimationFrame = 0;
    BaseOffset = Point(0, 0);
    DoingDrag = false;
    CachedDrawnImage.reset();

    if (ResetZoom)
        ImageZoom = 1.0f;

    if (IsAutoFit || IsInThumbnailMode)
        DoAutoFit();

    LastFrame = ClockType::now();
}

void SuperViewer::_SetLoadedImage(std::shared_ptr<LoadedImage> image)
{
    DisplayImage = image;
    IsImageReady = false;

    // Force redraw //
    queue_draw();
}

// ------------------------------------ //
void SuperViewer::OpenImageInNewWindow()
{
    if (!DisplayedResource)
        return;

    DualView::Get().OpenImageViewer(DisplayedResource, ScrollableImages);
}

// ------------------------------------ //
bool SuperViewer::MoveInCollection(bool forwards, bool wrap /*= true*/)
{
    if (!ScrollableImages)
        return false;

    if (USE_EXTRA_VIEWER_LOGGING)
        LOG_INFO("SuperViewer: moving in collection");

    auto nextimage = forwards ? ScrollableImages->GetNextImage(DisplayedResource, wrap) :
                                ScrollableImages->GetPreviousImage(DisplayedResource, wrap);

    if (!nextimage)
        return false;

    SetImage(nextimage);
    return true;
}

void SuperViewer::SetImageList(std::shared_ptr<ImageListScroll> list)
{
    ScrollableImages = list;
}

void SuperViewer::RegisterSetImageNotify(std::function<void()> callback)
{
    ImageChangeCallback = callback;
}

// ------------------------------------ //
void SuperViewer::_AddRedrawTimer(int32_t ms)
{
    if (CurrentTimer == ms)
        return;

    CurrentTimer = ms;

    if (ms <= 0)
    {
        // No new timer wanted //
        return;
    }

    // Add redraw timer //
    Glib::signal_timeout().connect(
        sigc::bind<int32_t>(sigc::mem_fun(*this, &SuperViewer::_OnTimerCheck), ms), CurrentTimer);
}

bool SuperViewer::_OnTimerCheck(int32_t currenttime)
{
    // If currenttime is no longer CurrentTimer, we have been replaced //
    if (currenttime != CurrentTimer)
    {
        return false;
    }

    // Cause a redraw //
    queue_draw();

    return true;
}

void SuperViewer::_AddUnloadTimer()
{
    if (HasUnloadTimer)
        return;

    HasUnloadTimer = true;

    // Unload timer //
    Glib::signal_timeout().connect(sigc::mem_fun(*this, &SuperViewer::_OnUnloadTimer), SUPER_UNLOAD_IMAGE_AFTER_MS);
}

bool SuperViewer::_OnUnloadTimer()
{
    if (HasBeenDrawn)
    {
        HasBeenDrawn = false;
        return true;
    }

    // Unload it //
    if (CachedDrawnImage)
        CachedDrawnImage.reset();
    HasUnloadTimer = false;
    return false;
}

void SuperViewer::_OnUnMapped()
{
    HasBeenDrawn = false;

    // Prevents this being called after destructor
    if (DisplayedResource)
    {
        CachedDrawnImage.reset();
    }
}

// ------------------------------------ //
bool SuperViewer::_OnMouseMove(GdkEventMotion* motion_event)
{
    if (!((int)Events & (int)ENABLED_EVENTS::DRAG) || (!CanStartDrag && !DoingDrag))
        return false;

    Point mousePos = Point(motion_event->x, motion_event->y);

    // Check can start //
    if (!DoingDrag && CanStartDrag)
    {
        if ((DragStartPos - mousePos).HAddAbs() > 8)
        {
            DoingDrag = true;
            OffsetBeforeDrag = BaseOffset;
            return true;
        }
    }

    if (DoingDrag)
    {
        CanStartDrag = false;

        BaseOffset = OffsetBeforeDrag;

        BaseOffset += (mousePos - DragStartPos);

        queue_draw();
        return true;
    }

    return false;
}

bool SuperViewer::_OnMouseButtonPressed(GdkEventButton* event)
{
    if (!((int)Events & (int)ENABLED_EVENTS::DRAG) && !((int)Events & (int)ENABLED_EVENTS::POPUP))
    {
        return false;
    }

    // Double click
    if (event->type == GDK_2BUTTON_PRESS)
    {
        if ((int)Events & (int)ENABLED_EVENTS::POPUP)
        {
            OpenImageInNewWindow();
            return true;
        }
    }

    // Left mouse //
    if (event->button == 1)
    {
        CanStartDrag = true;
        DragStartPos = Point(event->x, event->y);
    }

    return false;
}

bool SuperViewer::_OnMouseButtonReleased(GdkEventButton* event)
{
    if (!((int)Events & (int)ENABLED_EVENTS::DRAG))
        return false;

    // Left mouse //
    if (event->button == 1)
    {
        if (!DoingDrag)
        {
            // Single click //
        }

        DoingDrag = false;
        CanStartDrag = false;
    }

    return false;
}

bool SuperViewer::_OnKeyPressed(GdkEventKey* event)
{
    if (!((int)Events & (int)ENABLED_EVENTS::MOVE_KEYS))
        return false;

    switch (event->keyval)
    {
        case GDK_KEY_Left:
        {
            MoveInCollection(false, true);
            return true;
        }
        case GDK_KEY_Right:
        {
            MoveInCollection(true, true);
            return true;
        }
        default:
            return false;
    }

    return false;
}

bool SuperViewer::_OnScroll(GdkEventScroll* event)
{
    if (!((int)Events & (int)ENABLED_EVENTS::SCROLL))
        return false;

    if (!IsImageReady)
        return false;

    float scrollAmount = 1.f;

    // Setup the scroll amount //
    if (event->delta_x == 0 && event->delta_y == 0)
    {
        // Just the direction //
        scrollAmount = event->direction == GDK_SCROLL_UP ? 1.08f : 0.92f;
    }
    else
    {
        // Multiply with the delta //
        scrollAmount = 1.f + (event->delta_y / 750.f);
    }

    if (BaseOffset.X != 0 || BaseOffset.Y != 0)
    {
        Point mousepos = Point(event->x, event->y);

        int theight = (int)(DisplayImage->GetHeight() * ImageZoom);
        int twidth = (int)(DisplayImage->GetWidth() * ImageZoom);

        Point topleft = CalculateImageRenderTopLeft(DisplayImage->GetWidth(), DisplayImage->GetHeight(), ImageZoom);

        Point imagesizerelative =
            Point((mousepos.X - topleft.X) / (float)twidth, (mousepos.Y - topleft.Y) / (float)theight);

        ImageZoom *= scrollAmount;

        theight = (int)(DisplayImage->GetHeight() * ImageZoom);
        twidth = (int)(DisplayImage->GetWidth() * ImageZoom);

        Point newtopleft = CalculateImageRenderTopLeft(DisplayImage->GetWidth(), DisplayImage->GetHeight(), ImageZoom);

        Point newimagesizerelative =
            Point((mousepos.X - newtopleft.X) / (float)twidth, (mousepos.Y - newtopleft.Y) / (float)theight);

        Point difference =
            Point(imagesizerelative.X - newimagesizerelative.X, imagesizerelative.Y - newimagesizerelative.Y);

        Point pixeldifference = Point((int)(difference.X * twidth), (int)(difference.Y * theight));

        BaseOffset.X -= pixeldifference.X;
        BaseOffset.Y -= pixeldifference.Y;

        // Verification code //
        Point verifyleft = CalculateImageRenderTopLeft(DisplayImage->GetWidth(), DisplayImage->GetHeight(), ImageZoom);

        Point verifyrelative =
            Point((mousepos.X - verifyleft.X) / (float)twidth, (mousepos.Y - verifyleft.Y) / (float)theight);

        LEVIATHAN_ASSERT(std::abs(verifyrelative.X - imagesizerelative.X) < 1.001f, "Invalid math assumption");
        LEVIATHAN_ASSERT(std::abs(verifyrelative.Y - imagesizerelative.Y) < 1.001f, "Invalid math assumption");
    }
    else
    {
        ImageZoom *= scrollAmount;
    }

    IsAutoFit = false;
    queue_draw();
    return true;
}

void SuperViewer::_OnResize(Gtk::Allocation& allocation)
{
    if (!IsImageReady)
        return;

    if (IsAutoFit || IsInThumbnailMode)
        DoAutoFit();
}
