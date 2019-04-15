// ------------------------------------ //
#include "Reorder.h"

#include "resources/Collection.h"

#include "DualView.h"

using namespace DV;
// ------------------------------------ //
ReorderWindow::ReorderWindow(const std::shared_ptr<Collection>& collection) :
    ResetResults("Reset Changes"), MainContainer(Gtk::ORIENTATION_VERTICAL),
    WorkspaceLabel("Workspace:"), SelectAllInWorkspace("_Select All", true),
    TopLeftSide(Gtk::ORIENTATION_VERTICAL), LastSelectedLabel("Last Selected Image"),
    LastSelectedImage(nullptr, SuperViewer::ENABLED_EVENTS::ALL, false),
    TopRightSide(Gtk::ORIENTATION_VERTICAL), CurrentImageOrder("Current image order:"),
    RemoveSelected("Remove Selected"),
    OpenSelectedInImporter("_Open Selected In Importer", true), ApplyButton("_Apply", true),
    TargetCollection(collection)
{
    LEVIATHAN_ASSERT(collection, "reorder window must be given a collection");

    signal_delete_event().connect(sigc::mem_fun(*this, &ReorderWindow::_OnClosed));

    set_default_size(900, 600);
    property_resizable() = true;

    //
    // Header bar setup
    //
    HeaderBar.property_title() = "Reorder collection";
    HeaderBar.property_subtitle() = collection->GetName();
    HeaderBar.property_show_close_button() = true;

    Menu.set_image_from_icon_name("open-menu-symbolic");

    ResetResults.property_relief() = Gtk::RELIEF_NONE;
    ResetResults.signal_clicked().connect(sigc::mem_fun(*this, &ReorderWindow::Reset));
    MenuPopover.Container.pack_start(ResetResults);

    MenuPopover.show_all_children();

    Menu.set_popover(MenuPopover);

    HeaderBar.pack_end(Menu);

    Redo.set_image_from_icon_name("edit-redo-symbolic");
    HeaderBar.pack_end(Redo);
    Redo.property_sensitive() = false;

    Undo.set_image_from_icon_name("edit-undo-symbolic");
    HeaderBar.pack_end(Undo);
    Undo.property_sensitive() = false;

    set_titlebar(HeaderBar);

    //
    // Window contents start here
    //
    WorkspaceLabel.property_valign() = Gtk::ALIGN_END;
    VeryTopLeftContainer.pack_start(WorkspaceLabel, false, false);
    VeryTopLeftContainer.pack_end(SelectAllInWorkspace, false, false);

    TopLeftSide.pack_start(VeryTopLeftContainer, false, false);

    Workspace.property_hexpand() = true;
    Workspace.property_vexpand() = true;
    Workspace.set_min_content_height(200);
    Workspace.set_min_content_width(500);
    WorkspaceFrame.add(Workspace);
    TopLeftSide.pack_start(WorkspaceFrame, true, true);

    // Middle buttons
    CurrentImageOrder.property_valign() = Gtk::ALIGN_END;
    MiddleBox.pack_start(CurrentImageOrder, false, false);

    UpArrow.set_image_from_icon_name("go-up-symbolic");
    UpArrow.get_style_context()->add_class("ArrowButton");
    MiddleBox.pack_end(UpArrow, false, false);

    DownArrow.set_image_from_icon_name("go-down-symbolic");
    DownArrow.get_style_context()->add_class("ArrowButton");
    MiddleBox.pack_end(DownArrow, false, false);

    MiddleBox.set_spacing(3);
    TopLeftSide.pack_end(MiddleBox, false, false);

    TopContainer.property_spacing() = 15;
    TopContainer.pack_start(TopLeftSide, true, true);


    // Top right side (the left side includes the buttons below it)
    LastSelectedLabel.property_valign() = Gtk::ALIGN_END;
    // FirstSelected.property_vexpand() = false;
    TopRightSide.pack_start(LastSelectedLabel, false, false);
    LastSelectedImage.property_height_request() = 200;
    TopRightSide.pack_end(LastSelectedImage, true, true);

    TopContainer.pack_end(TopRightSide, true, true);

    MainContainer.pack_start(TopContainer, true, true);


    ImageList.property_hexpand() = true;
    ImageList.property_vexpand() = true;
    ImageList.set_min_content_height(250);

    ImageListFrame.add(ImageList);
    MainContainer.pack_start(ImageListFrame, true, true);

    // Bottom buttons
    RemoveSelected.get_style_context()->add_class("destructive-action");
    RemoveSelected.signal_clicked().connect(
        sigc::mem_fun(*this, &ReorderWindow::_DeleteSelectedPressed));
    BottomButtons.pack_start(RemoveSelected, false, false);

    OpenSelectedInImporter.signal_clicked().connect(
        sigc::mem_fun(*this, &ReorderWindow::_OpenSelectedInImporterPressed));
    BottomButtons.pack_start(OpenSelectedInImporter, false, false);

    ApplyButton.set_image_from_icon_name("emblem-ok-symbolic");
    ApplyButton.set_always_show_image(true);
    ApplyButton.signal_clicked().connect(sigc::mem_fun(*this, &ReorderWindow::Apply));
    BottomButtons.pack_end(ApplyButton, false, false);

    BottomButtons.set_spacing(3);
    MainContainer.pack_end(BottomButtons, false, false);


    // Add the main container
    MainContainer.set_spacing(3);
    add(MainContainer);

    show_all_children();

    Reset();
}

ReorderWindow::~ReorderWindow()
{
    Close();
}

bool ReorderWindow::_OnClosed(GdkEventAny* event)
{
    if(DoneChanges) {

        auto dialog = Gtk::MessageDialog(*this, "Discard changes?", false,
            Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);

        dialog.set_secondary_text(
            "You have made unsaved changes. Closing this window will discard them.", true);
        int result = dialog.run();

        if(result != Gtk::RESPONSE_YES) {

            return true;
        }
    }

    set_sensitive(false);

    // Continue with close
    return false;
}

void ReorderWindow::_OnClose() {}
// ------------------------------------ //
void ReorderWindow::Apply()
{
    if(!DoneChanges) {
        // No changes have been done
        close();
        return;
    }

    set_sensitive(false);

    // Show loading cursor
    auto window = get_window();
    if(window)
        window->set_cursor(Gdk::Cursor::create(window->get_display(), Gdk::WATCH));

    // Apply the change
    LOG_WRITE("TODO: apply change");

    // And then close this
    // This is set to false to skip any questions
    DoneChanges = false;
    close();
}
// ------------------------------------ //
void ReorderWindow::Reset()
{
    History.Clear();
    Workspace.Clear();
    LastSelectedImage.RemoveImage();
    ImageList.Clear();

    _UpdateButtonStatus();

    // Start loading the new data
    // Show loading cursor
    auto window = get_window();
    if(window)
        window->set_cursor(Gdk::Cursor::create(window->get_display(), Gdk::WATCH));

    set_sensitive(false);

    auto isalive = GetAliveMarker();

    DualView::Get().QueueDBThreadFunction([=, collection = TargetCollection]() {
        auto images = collection->GetImages();

        DualView::Get().InvokeFunction([this, isalive, images]() {
            INVOKE_CHECK_ALIVE_MARKER(isalive);

            // Data loading finished

            CollectionImages = images;
            _UpdateShownItems();

            set_sensitive(true);

            // Restore cursor
            auto window = get_window();
            if(window)
                window->set_cursor();
        });
    });
}
// ------------------------------------ //
std::vector<std::shared_ptr<Image>> ReorderWindow::GetSelected() const
{
    std::vector<std::shared_ptr<ResourceWithPreview>> items;

    ImageList.GetSelectedItems(items);

    std::vector<std::shared_ptr<Image>> casted;
    casted.reserve(items.size());

    for(const auto& item : items) {
        auto castedItem = std::dynamic_pointer_cast<Image>(item);

        if(castedItem)
            casted.push_back(castedItem);
    }

    return casted;
}
// ------------------------------------ //
bool ReorderWindow::PerformAction(HistoryItem& action)
{
    return false;
}

bool ReorderWindow::UndoAction(HistoryItem& action)
{
    return false;
}
// ------------------------------------ //
void ReorderWindow::_UpdateButtonStatus()
{
    Undo.property_sensitive() = History.CanUndo();
    Redo.property_sensitive() = History.CanRedo();

    SelectAllInWorkspace.property_sensitive() = !Workspace.IsEmpty();

    DownArrow.property_sensitive() = Workspace.CountSelectedItems() > 0;

    bool selectedInLower = ImageList.CountSelectedItems() > 0;

    UpArrow.property_sensitive() = selectedInLower;
    RemoveSelected.property_sensitive() = selectedInLower;
    OpenSelectedInImporter.property_sensitive() = selectedInLower;
}
// ------------------------------------ //
void ReorderWindow::_OpenSelectedInImporterPressed()
{
    DualView::Get().OpenImporter(GetSelected());
}

void ReorderWindow::_DeleteSelectedPressed()
{
    const auto selected = GetSelected();
    if(selected.empty())
        return;

    auto dialog = Gtk::MessageDialog(*this, "Remove selected images from this collection?",
        false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);

    dialog.set_secondary_text(
        "This action can only be undone from the action undo window. And this view needs to "
        "be reset in order to restore the images.");
    int result = dialog.run();

    if(result != Gtk::RESPONSE_YES) {

        return;
    }

    // Do this in the background to not interrupt the user
    DualView::Get().QueueDBThreadFunction([collection = TargetCollection, selected]() {
        if(!collection->RemoveImage(selected)) {

            LOG_ERROR("ReorderWindow: failed to remove selected from current collection");
        }
    });

    CollectionImages.erase(std::remove_if(CollectionImages.begin(), CollectionImages.end(),
                               [&](const std::shared_ptr<Image>& image) {
                                   return std::find(selected.begin(), selected.end(), image) !=
                                          selected.end();
                               }),
        CollectionImages.end());

    _UpdateShownItems();
}
// ------------------------------------ //
void ReorderWindow::_UpdateShownItems()
{
    ImageList.SetShownItems(CollectionImages.begin(), CollectionImages.end(),
        std::make_shared<ItemSelectable>([=](ListItem& item) { _UpdateButtonStatus(); }));

    _UpdateButtonStatus();
}


// ------------------------------------ //
// ReorderWindow::HistoryItem
bool ReorderWindow::HistoryItem::DoRedo()
{
    return Target.PerformAction(*this);
}

bool ReorderWindow::HistoryItem::DoUndo()
{
    return Target.UndoAction(*this);
}
