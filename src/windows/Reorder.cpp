// ------------------------------------ //
#include "Reorder.h"

#include "resources/Collection.h"

using namespace DV;
// ------------------------------------ //
ReorderWindow::ReorderWindow(const std::shared_ptr<Collection>& collection) :
    ResetResults("Reset Changes"), MainContainer(Gtk::ORIENTATION_VERTICAL),
    WorkspaceLabel("Workspace:"), SelectAllInWorkspace("_Select All", true),
    TopLeftSide(Gtk::ORIENTATION_VERTICAL), LastSelectedLabel("Last Selected Image"),
    LastSelectedImage(nullptr, SuperViewer::ENABLED_EVENTS::ALL, false),
    TopRightSide(Gtk::ORIENTATION_VERTICAL), CurrentImageOrder("Current image order:"),
    RemoveSelected("Remove Selected"),
    OpenSelectedInImporter("_Open Selected In Importer", true), Apply("_Apply", true)
{
    LEVIATHAN_ASSERT(collection, "reorder window must be given a collection");

    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));

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
    BottomButtons.pack_start(RemoveSelected, false, false);
    BottomButtons.pack_start(OpenSelectedInImporter, false, false);

    Apply.set_image_from_icon_name("emblem-ok-symbolic");
    Apply.set_always_show_image(true);

    BottomButtons.pack_end(Apply, false, false);

    BottomButtons.set_spacing(3);
    MainContainer.pack_end(BottomButtons, false, false);


    // Add the main container
    MainContainer.set_spacing(3);
    add(MainContainer);

    show_all_children();
}

ReorderWindow::~ReorderWindow()
{
    Close();
}

void ReorderWindow::_OnClose() {}
// ------------------------------------ //
