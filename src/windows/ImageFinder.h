#pragma once

#include "BaseWindow.h"
#include "IsAlive.h"

namespace DV {

class SuperContainer;

//! \brief Window for searching for individual images (and collections)
class ImageFinder : public BaseWindow, public Gtk::Window, public IsAlive {
public:
    ImageFinder(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder);
    ~ImageFinder();


    //! Searches again
    void OnSearchChanged();

protected:
    void _OnClose() override;

    //! Updates the status spinner
    void _SetSearchingState(bool active);

    //! Called when searching for images has failed
    void _OnFailSearch(const std::string& message);

private:
    SuperContainer* Container = nullptr;

    Gtk::Entry* MainSearchBar;

    Gtk::Spinner* SearchActiveSpinner;

    Gtk::Label* FoundImageCountLabel;

    Gtk::Label* SelectStatusLabel;
};
} // namespace DV
