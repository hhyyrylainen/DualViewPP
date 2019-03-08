// ------------------------------------ //
#include "Reorder.h"

#include "resources/Collection.h"

using namespace DV;
// ------------------------------------ //
ReorderWindow::ReorderWindow(const std::shared_ptr<Collection>& collection)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));
}

ReorderWindow::~ReorderWindow()
{
    Close();
}

void ReorderWindow::_OnClose() {}
// ------------------------------------ //
