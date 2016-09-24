// ------------------------------------ //
#include "SuperViewer.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //

SuperViewer::SuperViewer(_GtkDrawingArea* area, Glib::RefPtr<Gtk::Builder> builder,
    std::shared_ptr<Image> displayedResource) : Gtk::DrawingArea(area)
{
    // Add redraw timer //
    Glib::signal_timeout().connect(sigc::mem_fun(*this, &SuperViewer::_OnTimerCheck),
        CurrentTimer);
    
    // Do setup stuff //
    DisplayImage = displayedResource->GetImage();
}

SuperViewer::~SuperViewer(){

    LOG_WRITE("SUper destructed");
}
// ------------------------------------ //
bool SuperViewer::on_draw(const Cairo::RefPtr<Cairo::Context>& cr){

    Gtk::Allocation allocation = get_allocation();
    const int width = allocation.get_width();
    const int height = allocation.get_height();

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

        if(!DisplayImage->IsValid()){

            // Draw error message //
            
        } else {
        
            // Draw positioned image //
            auto img = DisplayImage->CreateGtkImage();

            Gdk::Cairo::set_source_pixbuf(cr, img, 0, 0);
            cr->rectangle(0, 0, img->get_width(), img->get_height());
            cr->fill();
        }
    }
    
    return true;
}
// ------------------------------------ //
bool SuperViewer::IsImageReadyToShow() const{

    if(!DisplayImage)
        return false;

    return DisplayImage->IsLoaded();
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

