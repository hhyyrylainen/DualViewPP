// ------------------------------------ //
#include "SuperViewer.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //

SuperViewer::SuperViewer(_GtkDrawingArea* area, Glib::RefPtr<Gtk::Builder> builder,
    std::shared_ptr<Image> displayedResource) : Gtk::DrawingArea(area)
{

    // Do setup stuff //


}
// ------------------------------------ //
bool SuperViewer::on_draw(const Cairo::RefPtr<Cairo::Context>& cr){

    const Gtk::Allocation allocation = get_allocation();
    const double scale_x = 1;
    const double scale_y = 1;
    auto refStyleContext = get_style_context();

    // paint the background
    refStyleContext->render_background(cr,
        allocation.get_x(), allocation.get_y(),
        allocation.get_width(), allocation.get_height());

    // draw the foreground
    const auto state = refStyleContext->get_state();
    Gdk::Cairo::set_source_rgba(cr, refStyleContext->get_color(state));
    cr->move_to(155.*scale_x, 165.*scale_y);
    cr->line_to(155.*scale_x, 838.*scale_y);
    cr->line_to(265.*scale_x, 900.*scale_y);
    cr->line_to(849.*scale_x, 564.*scale_y);
    cr->line_to(849.*scale_x, 438.*scale_y);
    cr->line_to(265.*scale_x, 100.*scale_y);
    cr->line_to(155.*scale_x, 165.*scale_y);
    cr->move_to(265.*scale_x, 100.*scale_y);
    cr->line_to(265.*scale_x, 652.*scale_y);
    cr->line_to(526.*scale_x, 502.*scale_y);
    cr->move_to(369.*scale_x, 411.*scale_y);
    cr->line_to(633.*scale_x, 564.*scale_y);
    cr->move_to(369.*scale_x, 286.*scale_y);
    cr->line_to(369.*scale_x, 592.*scale_y);
    cr->move_to(369.*scale_x, 286.*scale_y);
    cr->line_to(849.*scale_x, 564.*scale_y);
    cr->move_to(633.*scale_x, 564.*scale_y);
    cr->line_to(155.*scale_x, 838.*scale_y);
    cr->stroke();
    
    return true;
}

// ------------------------------------ //
void SuperViewer::SetupCairoContext(){

    if(DrawingContext){

        LOG_WARNING("Trying to Create Cairo Context again, ignoring");
        return;
    }

    LEVIATHAN_ASSERT(get_window(), "Trying to Create Cairo context without a window");

    DrawingContext = get_window()->create_cairo_context();

    LEVIATHAN_ASSERT(DrawingContext, "Failed to acquire Cairo context");
    
    DrawingContext->set_source_rgb(1.0, 0.0, 0.0); 
    DrawingContext->set_line_width(2.0); 
}
