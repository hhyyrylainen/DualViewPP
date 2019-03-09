// ------------------------------------ //
#include "DuplicateFinder.h"

using namespace DV;
// ------------------------------------ //
DuplicateFinderWindow::DuplicateFinderWindow() :
    ScanControl("Start"), ResetResults("Reset Results"), SensitivityLabel("Sensitivity"),
    Sensitivity(Gtk::ORIENTATION_HORIZONTAL), MainContainer(Gtk::ORIENTATION_VERTICAL),
    ProgressContainer(Gtk::ORIENTATION_VERTICAL), ProgressLabel("Scan not started"),
    CurrentlyShownGroup("No duplicates found"), ImagesContainer(Gtk::ORIENTATION_HORIZONTAL),
    ImagesLeftSide(Gtk::ORIENTATION_VERTICAL), FirstSelected("First Selected"),
    FirstImage(nullptr, SuperViewer::ENABLED_EVENTS::ALL_BUT_MOVE, false),
    ImagesRightSide(Gtk::ORIENTATION_VERTICAL), LastSelected("Last Selected"),
    LastImage(nullptr, SuperViewer::ENABLED_EVENTS::ALL_BUT_MOVE, false),
    ImageListAreaContainer(Gtk::ORIENTATION_HORIZONTAL),
    ImageListLeftSide(Gtk::ORIENTATION_VERTICAL),
    ImageListLeftTop(Gtk::ORIENTATION_HORIZONTAL), DuplicateImagesLabel("Duplicate Images"),
    DeleteSelectedAfterFirst("Delete Selected After First"),
    BottomRightContainer(Gtk::ORIENTATION_VERTICAL),
    DeleteAllAfterFirst("Delete All After First"), NotDuplicates("Not Duplicates"),
    Skip("Skip")
{
    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));

    set_default_size(600, 800);
    property_resizable() = true;

    //
    // Header bar setup
    //
    HeaderBar.property_title() = "Duplicate Finder";
    HeaderBar.property_show_close_button() = true;

    Menu.set_image_from_icon_name("open-menu-symbolic");

    ResetResults.property_relief() = Gtk::RELIEF_NONE;
    MenuPopover.Container.pack_start(ResetResults);
    MenuPopover.Container.pack_start(Separator1);
    MenuPopover.Container.pack_start(SensitivityLabel);

    Sensitivity.set_digits(0);
    Sensitivity.set_range(1, 100);
    Sensitivity.property_draw_value() = false;
    Sensitivity.property_has_origin() = true;
    Sensitivity.add_mark(90, Gtk::POS_BOTTOM, "default");
    // TODO: hook this up
    Sensitivity.set_value(90);
    MenuPopover.Container.pack_start(Sensitivity);

    MenuPopover.show_all_children();

    Menu.set_popover(MenuPopover);

    HeaderBar.pack_end(Menu);

    Redo.set_image_from_icon_name("edit-redo-symbolic");
    HeaderBar.pack_end(Redo);
    Redo.property_sensitive() = false;

    Undo.set_image_from_icon_name("edit-undo-symbolic");
    HeaderBar.pack_end(Undo);
    Undo.property_sensitive() = false;

    ScanControl.set_can_default(true);
    ScanControl.get_style_context()->add_class("suggested-action");
    HeaderBar.pack_start(ScanControl);

    set_titlebar(HeaderBar);

    //
    // Window contents start here
    //

    ScanProgress.property_fraction() = 0.f;
    ProgressContainer.add(ScanProgress);
    ProgressContainer.add(ProgressLabel);

    MainContainer.pack_start(ProgressContainer, false, false);

    Separator2.property_margin_top() = 5;
    Separator2.property_height_request() = 3;
    Separator2.property_margin_bottom() = 2;

    MainContainer.pack_start(Separator2, false, false);

    MainContainer.pack_start(CurrentlyShownGroup, false, false);

    // Images
    FirstSelected.property_valign() = Gtk::ALIGN_END;
    FirstSelected.property_vexpand() = false;
    ImagesLeftSide.pack_start(FirstSelected, false, false);
    FirstImage.property_height_request() = 300;
    ImagesLeftSide.pack_end(FirstImage, true, true);
    ImagesContainer.pack_start(ImagesLeftSide, true, true);

    LastSelected.property_valign() = Gtk::ALIGN_END;
    LastSelected.property_vexpand() = false;
    ImagesRightSide.pack_start(LastSelected, false, false);
    LastImage.property_height_request() = 300;
    ImagesRightSide.pack_end(LastImage, true, true);
    ImagesContainer.pack_start(ImagesRightSide, true, true);

    ImagesContainer.set_spacing(5);

    MainContainer.pack_start(ImagesContainer, true, true);

    // Bottom part

    // Left
    DuplicateImagesLabel.property_valign() = Gtk::ALIGN_END;
    DuplicateImagesLabel.property_margin_start() = 2;
    ImageListLeftTop.pack_start(DuplicateImagesLabel, false, false);
    DeleteSelectedAfterFirst.property_valign() = Gtk::ALIGN_CENTER;
    DeleteSelectedAfterFirst.property_hexpand() = false;
    ImageListLeftTop.pack_end(DeleteSelectedAfterFirst, false, false);

    ImageListLeftTop.set_spacing(15);
    ImageListLeftSide.pack_start(ImageListLeftTop, false, false);

    DuplicateGroupImages.property_hexpand() = true;
    DuplicateGroupImages.property_vexpand() = true;
    DuplicateGroupImages.set_min_content_height(200);
    DuplicateGroupImagesFrame.add(DuplicateGroupImages);

    ImageListLeftSide.pack_end(DuplicateGroupImagesFrame, true, true);

    ImageListAreaContainer.pack_start(ImageListLeftSide, true, true);

    // Right
    DeleteAllAfterFirst.property_valign() = Gtk::ALIGN_END;
    BottomRightContainer.pack_end(DeleteAllAfterFirst);
    NotDuplicates.property_valign() = Gtk::ALIGN_END;
    BottomRightContainer.pack_end(NotDuplicates);
    Skip.property_valign() = Gtk::ALIGN_END;
    BottomRightContainer.pack_end(Skip);

    BottomRightContainer.property_vexpand() = false;
    BottomRightContainer.property_hexpand() = false;
    BottomRightContainer.property_homogeneous() = false;
    BottomRightContainer.property_valign() = Gtk::ALIGN_END;
    BottomRightContainer.property_spacing() = 3;

    ImageListAreaContainer.pack_end(BottomRightContainer, false, false);

    ImageListAreaContainer.property_margin_top() = 5;
    MainContainer.pack_end(ImageListAreaContainer, true, true);

    add(MainContainer);

    show_all_children();
}

DuplicateFinderWindow::~DuplicateFinderWindow()
{
    Close();
}

void DuplicateFinderWindow::_OnClose() {}
// ------------------------------------ //
