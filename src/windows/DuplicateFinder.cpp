// ------------------------------------ //
#include "DuplicateFinder.h"

#include "Database.h"
#include "DualView.h"

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
    SensitivityLabel.property_tooltip_text() =
        "Higher sensitivity requires images to be more similar before reporting a duplicate";
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
    ScanControl.signal_clicked().connect(
        sigc::mem_fun(*this, &DuplicateFinderWindow::_ScanButtonPressed));
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
    DeleteSelectedAfterFirst.property_sensitive() = false;
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
    DeleteAllAfterFirst.property_sensitive() = false;
    BottomRightContainer.pack_end(DeleteAllAfterFirst);
    NotDuplicates.property_valign() = Gtk::ALIGN_END;
    NotDuplicates.property_sensitive() = false;
    BottomRightContainer.pack_end(NotDuplicates);
    Skip.property_valign() = Gtk::ALIGN_END;
    Skip.property_sensitive() = false;
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

    // Setup non widget stuff

    // Status listener for signature calculation
    Calculator.SetStatusListener(
        std::bind(&DuplicateFinderWindow::_ReportSignatureCalculationStatus, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

DuplicateFinderWindow::~DuplicateFinderWindow()
{
    Close();
}

void DuplicateFinderWindow::_OnClose()
{
    // Make sure the background thread is closed
    Calculator.Pause(true);
}
// ------------------------------------ //
void DuplicateFinderWindow::_ScanButtonPressed()
{
    constexpr auto DETECTION_STRING = "Detecting images needing signature calculation...";
    if(Scanning) {
        // Stop scanning
        Calculator.Pause();

    } else {
        DoneWithSignatures = false;
        ProgressLabel.property_label() = DETECTION_STRING;

        // Detect images to scan
        auto isalive = GetAliveMarker();

        DualView::Get().QueueDBThreadFunction([=]() {
            const auto imagesWithoutSignature =
                DualView::Get().GetDatabase().SelectImageIDsWithoutSignatureAG();

            LOG_INFO("Found " + std::to_string(imagesWithoutSignature.size()) +
                     " images to calculate signatures for");

            DualView::Get().InvokeFunction([this, isalive, imagesWithoutSignature]() {
                INVOKE_CHECK_ALIVE_MARKER(isalive);

                if(imagesWithoutSignature.empty()) {
                    DoneWithSignatures = true;
                } else {

                    Calculator.AddImages(imagesWithoutSignature);
                }

                _CheckScanStatus();
            });
        });

        // Start scanning
        Calculator.Resume();
    }

    // Update button state
    Scanning = !Scanning;

    if(Scanning) {

        ScanControl.property_label() = "Stop";
        ScanControl.get_style_context()->remove_class("suggested-action");

    } else {

        ScanControl.property_label() = "Start";
        ScanControl.get_style_context()->add_class("suggested-action");

        // Reset text if nothing is in progress
        if(ProgressLabel.property_label() == DETECTION_STRING)
            ProgressLabel.property_label() = "Start scan to get results";
    }
}
// ------------------------------------ //
void DuplicateFinderWindow::_CheckScanStatus()
{
    DualView::IsOnMainThreadAssert();

    if(DoneWithSignatures) {
        ProgressLabel.property_label() =
            "Signature calculation complete. Searching for duplicates";
        ScanProgress.property_fraction() = 0;
    }

    if(!QueryingDBForDuplicates && !NoMoreQueryResults) {
        // Query DB for duplicates
        LOG_INFO("Querying DB for potential duplicate images");
        QueryingDBForDuplicates = true;

        auto isalive = GetAliveMarker();

        DualView::Get().QueueDBThreadFunction([=]() {
            LOG_INFO("Found " + std::to_string(0) + " potential duplicates");

            DualView::Get().GetDatabase().SelectPotentialImageDuplicates();

            DualView::Get().InvokeFunction([this, isalive]() {
                INVOKE_CHECK_ALIVE_MARKER(isalive);

                LOG_INFO("Finished finding duplicates from db, count: ");


                NoMoreQueryResults = true;
                QueryingDBForDuplicates = false;
                _CheckScanStatus();
            });
        });
    }

    _UpdateDuplicateStatusLabel();
}

void DuplicateFinderWindow::_ReportSignatureCalculationStatus(
    int processed, int total, bool done)
{
    auto isalive = GetAliveMarker();

    DualView::Get().InvokeFunction([=]() {
        INVOKE_CHECK_ALIVE_MARKER(isalive);

        ProgressLabel.property_label() = "Calculated signatures for " +
                                         std::to_string(processed) + "/" +
                                         std::to_string(total) + " images";

        // The bar is also used for the duplicate checking phase
        if(total > 0 && !DoneWithSignatures)
            ScanProgress.property_fraction() = static_cast<float>(processed) / total;

        DoneWithSignatures = done;

        // Reset this to query the DB again once done (or hit some percentage done)
        NoMoreQueryResults = false;

        // TODO: also periodically check during scan

        if(done)
            _CheckScanStatus();
    });
}
// ------------------------------------ //
void DuplicateFinderWindow::_UpdateDuplicateStatusLabel()
{
    DualView::IsOnMainThreadAssert();

    if(DuplicateGroups.empty()) {

        CurrentlyShownGroup.property_label() = "No duplicates found";

    } else {

        CurrentlyShownGroup.property_label() =
            "Resolving duplicate group " +
            std::to_string(TotalGroupsFound + 1 - DuplicateGroups.size()) + " of " +
            std::to_string(TotalGroupsFound) + "";
    }
}