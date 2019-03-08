#pragma once

#include "BaseWindow.h"
#include "IsAlive.h"

#include <gtkmm.h>

#include <atomic>

namespace DV {

class Collection;

//! \brief Allows user to reorder images in a collection
class ReorderWindow : public BaseWindow, public Gtk::Window, public IsAlive {
public:
    ReorderWindow(const std::shared_ptr<Collection>& collection);
    ~ReorderWindow();

protected:
    void _OnClose() override;

private:
};

} // namespace DV
