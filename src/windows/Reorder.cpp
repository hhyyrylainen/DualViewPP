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
    Redo.signal_clicked().connect(sigc::mem_fun(*this, &ReorderWindow::_RedoPressed));
    Redo.property_sensitive() = false;
    HeaderBar.pack_end(Redo);

    Undo.set_image_from_icon_name("edit-undo-symbolic");
    Undo.signal_clicked().connect(sigc::mem_fun(*this, &ReorderWindow::_UndoPressed));
    Undo.property_sensitive() = false;
    HeaderBar.pack_end(Undo);


    set_titlebar(HeaderBar);

    //
    // Window contents start here
    //
    WorkspaceLabel.property_valign() = Gtk::ALIGN_END;
    VeryTopLeftContainer.pack_start(WorkspaceLabel, false, false);
    SelectAllInWorkspace.signal_clicked().connect(
        sigc::mem_fun(*this, &ReorderWindow::_SelectAllPressed));
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
    UpArrow.property_tooltip_text() = "Move selected to workspace";
    UpArrow.signal_clicked().connect(
        sigc::mem_fun(*this, &ReorderWindow::_MoveToWorkspacePressed));
    MiddleBox.pack_end(UpArrow, false, false);

    DownArrow.set_image_from_icon_name("go-down-symbolic");
    DownArrow.get_style_context()->add_class("ArrowButton");
    DownArrow.property_tooltip_text() = "Move selected in workspace to insert point";
    DownArrow.signal_clicked().connect(
        sigc::mem_fun(*this, &ReorderWindow::_MoveBackFromWorkspacePressed));
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
    ImageList.property_tooltip_text() = "Set insert point by clicking";

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
    ApplyButton.property_tooltip_text() = "Save the new order";
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
            "You have made unsaved changes. Closing this window will discard them.");
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
    // Warn about items in workspace
    auto dialog = Gtk::MessageDialog(*this, "Continue with items in workspace?", false,
        Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);

    dialog.set_secondary_text("You have items in the workspace. Continuing will keep these at "
                              "their previous positions");
    int result = dialog.run();

    if(result != Gtk::RESPONSE_YES) {

        return;
    }

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
    LastSelectedImage.SetImageList(nullptr);
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

    return _CastToImages(items);
}

std::vector<std::shared_ptr<Image>> ReorderWindow::GetSelectedInWorkspace() const
{
    std::vector<std::shared_ptr<ResourceWithPreview>> items;
    Workspace.GetSelectedItems(items);

    return _CastToImages(items);
}

std::vector<std::shared_ptr<Image>> ReorderWindow::_CastToImages(
    std::vector<std::shared_ptr<ResourceWithPreview>>& items)
{
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
    auto& source = _GetCollectionForMoveGroup(action.MovedFrom);
    auto& target = _GetCollectionForMoveGroup(action.MoveTo);
    action.MovedFromIndex.clear();

    // Store the original image indexes
    for(const auto& image : action.ImagesToMove) {

        for(size_t i = 0; i < source.size(); ++i) {

            if(source[i] == image) {

                action.MovedFromIndex.push_back(i);
                break;
            }
        }
    }

    if(action.MovedFromIndex.size() != action.ImagesToMove.size()) {

        LOG_ERROR("ReorderWindow: perform couldn't find all the images to move in the source "
                  "vector. When undoing images will be placed wrong");

        action.MovedFromIndex.resize(action.ImagesToMove.size(), -1);
    }

    size_t insertPoint = action.MoveTargetIndex;

    // Remove the old items while trying to keep the insert point correct
    for(const auto& image : action.ImagesToMove) {

        for(size_t i = 0; i < source.size(); ++i) {

            if(source[i] == image) {

                if(i < insertPoint)
                    --insertPoint;

                source.erase(source.begin() + i);
                break;
            }
        }
    }

    // Perform the move
    auto insertIterator =
        insertPoint < target.size() ? target.begin() + insertPoint : target.end();

    for(const auto& image : action.ImagesToMove) {

        // The + 1 here makes the next image to be inserted after and not before the just
        // inserted image
        insertIterator = target.insert(insertIterator, image) + 1;
    }

    _UpdateListsTouchedByAction(action);
    return true;
}

bool ReorderWindow::UndoAction(HistoryItem& action)
{
    auto& source = _GetCollectionForMoveGroup(action.MovedFrom);
    auto& target = _GetCollectionForMoveGroup(action.MoveTo);

    if(action.MovedFromIndex.empty() ||
        action.MovedFromIndex.size() != action.ImagesToMove.size()) {
        LOG_ERROR(
            "ReorderWindow: undo moved from index is empty / doesn't contain everything");
    }

    // First remove from the target
    target.erase(std::remove_if(target.begin(), target.end(),
                     [&](const std::shared_ptr<Image>& image) {
                         return std::find(action.ImagesToMove.begin(),
                                    action.ImagesToMove.end(),
                                    image) != action.ImagesToMove.end();
                     }),
        target.end());

    // Then add to the source trying to place all images at the right stored index. To do this
    // move by the lowest index first
    std::vector<std::tuple<size_t, std::shared_ptr<Image>>> inserts;
    inserts.reserve(action.ImagesToMove.size());

    for(size_t i = 0; i < action.ImagesToMove.size(); ++i) {

        if(i < action.MovedFromIndex.size()) {
            inserts.emplace_back(action.MovedFromIndex[i], action.ImagesToMove[i]);

        } else {
            inserts.emplace_back(-1, action.ImagesToMove[i]);
        }
    }

    std::stable_sort(inserts.begin(), inserts.end(),
        [](const std::tuple<size_t, const std::shared_ptr<Image>&>& first,
            const std::tuple<size_t, const std::shared_ptr<Image>&>& second) {
            return std::get<0>(first) < std::get<0>(second);
        });

    for(const auto& operation : inserts) {

        const auto& position = std::get<0>(operation);
        const auto& image = std::get<1>(operation);

        if(position >= source.size()) {

            source.push_back(image);

        } else {

            source.insert(source.begin() + position, image);
        }
    }

    _UpdateListsTouchedByAction(action);
    return true;
}

void ReorderWindow::_UpdateListsTouchedByAction(HistoryItem& action)
{
    if(action.MoveTo == MOVE_GROUP::Workspace || action.MovedFrom == MOVE_GROUP::Workspace)
        _UpdateShownWorkspaceItems();
    if(action.MoveTo == MOVE_GROUP::MainList || action.MovedFrom == MOVE_GROUP::MainList)
        _UpdateShownItems();
}
// ------------------------------------ //
std::vector<std::shared_ptr<Image>>& ReorderWindow::_GetCollectionForMoveGroup(
    MOVE_GROUP group)
{
    switch(group) {
    case MOVE_GROUP::MainList: return CollectionImages;
    case MOVE_GROUP::Workspace: return WorkspaceImages;
    }

    LEVIATHAN_ASSERT(false, "unreachable");
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

void ReorderWindow::_UpdateShownItems()
{
    ImageList.SetShownItems(CollectionImages.begin(), CollectionImages.end(),
        std::make_shared<ItemSelectable>([=](ListItem& item) {
            _UpdateButtonStatus();

            std::vector<std::shared_ptr<ResourceWithPreview>> selected;
            ImageList.GetSelectedItems(selected);

            if(selected.size() > 0) {
                LastSelectedImage.SetImage(std::dynamic_pointer_cast<Image>(selected.back()));
                LastSelectedImage.SetImageList(TargetCollection);
            } else {
                LastSelectedImage.RemoveImage();
                LastSelectedImage.SetImageList(nullptr);
            }
        }));

    LastSelectedImage.RemoveImage();
    LastSelectedImage.SetImageList(nullptr);
    _UpdateButtonStatus();
}

void ReorderWindow::_UpdateShownWorkspaceItems()
{
    Workspace.SetShownItems(WorkspaceImages.begin(), WorkspaceImages.end(),
        std::make_shared<ItemSelectable>([=](ListItem& item) { _UpdateButtonStatus(); }));

    _UpdateButtonStatus();
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
void ReorderWindow::_MoveToWorkspacePressed()
{
    auto action = std::make_shared<HistoryItem>(
        *this, MOVE_GROUP::MainList, GetSelected(), MOVE_GROUP::Workspace, -1);

    // Putting the action into the history performs it
    History.AddAction(action);

    _UpdateButtonStatus();
}

void ReorderWindow::_MoveBackFromWorkspacePressed()
{
    // TODO: insert position selecting
    size_t insertPosition = -1;

    auto action = std::make_shared<HistoryItem>(*this, MOVE_GROUP::Workspace,
        GetSelectedInWorkspace(), MOVE_GROUP::MainList, insertPosition);

    // Putting the action into the history performs it
    History.AddAction(action);

    _UpdateButtonStatus();

    // TODO: this could be handled better with regards to undo
    DoneChanges = true;
}

void ReorderWindow::_SelectAllPressed()
{
    Workspace.SelectAllItems();
}
// ------------------------------------ //
void ReorderWindow::_UndoPressed()
{
    try {
        if(!History.Undo())
            throw Leviathan::Exception("unknown error in undo");

    } catch(const Leviathan::Exception& e) {
        LOG_ERROR("Undo failed:");
        e.PrintToLog();
    }

    _UpdateButtonStatus();
}

void ReorderWindow::_RedoPressed()
{
    try {
        if(!History.Redo())
            throw Leviathan::Exception("unknown error in redo");

    } catch(const Leviathan::Exception& e) {
        LOG_ERROR("Redo failed:");
        e.PrintToLog();
    }

    _UpdateButtonStatus();
}
// ------------------------------------ //
// ReorderWindow::HistoryItem
ReorderWindow::HistoryItem::HistoryItem(ReorderWindow& target, MOVE_GROUP movedfrom,
    const std::vector<std::shared_ptr<Image>>& imagestomove, MOVE_GROUP moveto,
    size_t movetargetindex) :
    Target(target),
    MovedFrom(movedfrom), ImagesToMove(imagestomove), MoveTo(moveto),
    MoveTargetIndex(movetargetindex)
{}

bool ReorderWindow::HistoryItem::DoRedo()
{
    return Target.PerformAction(*this);
}

bool ReorderWindow::HistoryItem::DoUndo()
{
    return Target.UndoAction(*this);
}
