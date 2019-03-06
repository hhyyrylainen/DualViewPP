// ------------------------------------ //
#include "DuplicateFinder.h"

using namespace DV;
// ------------------------------------ //
DuplicateFinderWindow::DuplicateFinderWindow()
{
    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));
}

DuplicateFinderWindow::~DuplicateFinderWindow()
{
    Close();
}

void DuplicateFinderWindow::_OnClose() {}
// ------------------------------------ //
