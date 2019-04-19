#pragma once

#include <gtkmm.h>

namespace DV {

//! \brief Interface for providing dragging information
class ItemDragInformationProvider {
public:
    virtual ~ItemDragInformationProvider();

    virtual std::vector<Gtk::TargetEntry> GetDragTypes() = 0;
};
} // namespace DV
