// ------------------------------------ //
#include "DuplicateFinder.h"

#include "Database.h"
#include "DualView.h"
#include "resources/DatabaseAction.h"

using namespace DV;
// ------------------------------------ //
const auto PROGRESS_LABEL_INITIAL_TEXT = "Scan not started";

DuplicateFinderWindow::DuplicateFinderWindow() :
    ScanControl("Start"), ResetResults("Reset Results"),
    ClearNotDuplicates("Clear manually ignored duplicates"), SensitivityLabel("Sensitivity"),
    Sensitivity(Gtk::ORIENTATION_HORIZONTAL), MainContainer(Gtk::ORIENTATION_VERTICAL),
    ProgressContainer(Gtk::ORIENTATION_VERTICAL), ProgressLabel(PROGRESS_LABEL_INITIAL_TEXT),
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


    ResetResults.signal_clicked().connect(
        sigc::mem_fun(*this, &DuplicateFinderWindow::ResetState));
    ResetResults.property_relief() = Gtk::RELIEF_NONE;



    MenuPopover.Container.pack_start(ResetResults);

    ClearNotDuplicates.signal_clicked().connect(
        sigc::mem_fun(*this, &DuplicateFinderWindow::_ClearNotDuplicatesPressed));
    ClearNotDuplicates.property_relief() = Gtk::RELIEF_NONE;
    MenuPopover.Container.pack_start(ClearNotDuplicates);

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
    Redo.signal_clicked().connect(sigc::mem_fun(*this, &DuplicateFinderWindow::_RedoPressed));
    Redo.property_sensitive() = false;
    HeaderBar.pack_end(Redo);

    Undo.set_image_from_icon_name("edit-undo-symbolic");
    Undo.signal_clicked().connect(sigc::mem_fun(*this, &DuplicateFinderWindow::_UndoPressed));
    Undo.property_sensitive() = false;
    HeaderBar.pack_end(Undo);


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
    DeleteSelectedAfterFirst.signal_clicked().connect(
        sigc::mem_fun(*this, &DuplicateFinderWindow::_DeleteSelectedAfterFirstPressed));
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
    DeleteAllAfterFirst.signal_clicked().connect(
        sigc::mem_fun(*this, &DuplicateFinderWindow::_DeleteAllAfterFirstPressed));
    BottomRightContainer.pack_end(DeleteAllAfterFirst);
    NotDuplicates.property_valign() = Gtk::ALIGN_END;
    NotDuplicates.property_sensitive() = false;
    NotDuplicates.signal_clicked().connect(
        sigc::mem_fun(*this, &DuplicateFinderWindow::_NotDuplicatesPressed));
    BottomRightContainer.pack_end(NotDuplicates);
    Skip.property_valign() = Gtk::ALIGN_END;
    Skip.property_sensitive() = false;
    Skip.signal_clicked().connect(sigc::mem_fun(*this, &DuplicateFinderWindow::_SkipPressed));
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
bool DuplicateFinderWindow::PerformAction(HistoryItem& action)
{
    // Detect items to remove
    if(action.GroupsVectorIndexToRemoveAt >= DuplicateGroups.size())
        return false;

    action.StoredShownDuplicateGroup = ShownDuplicateGroup;

    auto& group = DuplicateGroups[action.GroupsVectorIndexToRemoveAt];

    std::vector<std::shared_ptr<Image>> confirmedRemoved;
    confirmedRemoved.reserve(action.RemovedImages.size());

    // Remove the items specified by the action and check which images actually existed to
    // avoid problems in applying merges
    for(auto iter = group.begin(); iter != group.end();) {

        bool found = std::find(action.RemovedImages.begin(), action.RemovedImages.end(),
                         *iter) != action.RemovedImages.end();

        if(found) {

            confirmedRemoved.push_back(*iter);
            iter = group.erase(iter);
        } else {

            ++iter;
        }
    }

    // Actions with extra things
    // We don't return false here as we have already performed a part of the action and that
    // would need code to undo that first before this part is allowed to bail out
    switch(action.Type) {
    case ACTION_TYPE::Merge: {
        if(group.size() < 1) {

            LOG_ERROR("Cannot perform Image merge as target group is empty after removes");

        } else {

            if(action.AdditionalAction && action.AdditionalAction->IsPerformed()) {

                LOG_ERROR("DuplicateFinder: Redo: abandoning performed action!");
            }

            // If the already created action is fine we don't want to recreate it
            auto casted = std::dynamic_pointer_cast<ImageMergeAction>(action.AdditionalAction);

            // TODO: catch exceptions
            if(casted && casted->IsSame(*group.front(), confirmedRemoved)) {

                LOG_INFO("DuplicateFinder: Redo: reusing existing action, id: " +
                         std::to_string(casted->GetID()));
                if(!action.AdditionalAction->Redo()) {
                    LOG_ERROR("DuplicateFinder: Redo: existing extra action failed to redo");
                }
            } else {

                action.AdditionalAction = DualView::Get().GetDatabase().MergeImages(
                    *group.front(), confirmedRemoved);
            }
        }

        break;
    }
    case ACTION_TYPE::NotDuplicate: {

        auto ignorePairs = action.GenerateIgnorePairs();

        DualView::Get().GetDatabase().InsertIgnorePairs(ignorePairs);
        break;
    }

    case ACTION_TYPE::Remove: break;
    }

    // If the group only has a single item (or none) left remove it as well and move to the
    // next
    if(group.size() == 1) {

        // Add the removed image to the extra (if not already)
        if(std::find(action.ExtraRemovedGroupImages.begin(),
               action.ExtraRemovedGroupImages.end(),
               group.front()) == action.ExtraRemovedGroupImages.end())
            action.ExtraRemovedGroupImages.push_back(group.front());
        group.pop_back();
    }

    if(group.empty()) {

        DuplicateGroups.erase(DuplicateGroups.begin() + action.GroupsVectorIndexToRemoveAt);

        if(!DuplicateGroups.empty()) {

            _BrowseDuplicates(
                std::min(ShownDuplicateGroup, static_cast<int>(DuplicateGroups.size()) - 1));

        } else {
            _UpdateDuplicateWidgets();
        }
    }

    return true;
}

bool DuplicateFinderWindow::UndoAction(HistoryItem& action)
{
    if(action.GroupsVectorIndexToRemoveAt > DuplicateGroups.size())
        return false;

    // Undo any additional action
    if(action.AdditionalAction) {

        if(!action.AdditionalAction->IsPerformed()) {
            LOG_ERROR(
                "DuplicateFinder: Undo: additional action exists but it is not performed");
        } else {

            LOG_INFO("Undoing additional action");
            if(!action.AdditionalAction->Undo()) {
                LOG_ERROR("DuplicateFinder: Undo: failed to undo additional action");
                return false;
            }
        }
    }

    // Special actions based on type
    switch(action.Type) {
    case ACTION_TYPE::NotDuplicate: {
        auto ignorePairs = action.GenerateIgnorePairs();

        DualView::Get().GetDatabase().DeleteIgnorePairs(ignorePairs);
        break;
    }
    default: break;
    }

    // Restore a fully removed group
    if(action.GroupsVectorIndexToRemoveAt == DuplicateGroups.size()) {
        DuplicateGroups.push_back({});
    }

    auto& group = DuplicateGroups[action.GroupsVectorIndexToRemoveAt];

    // Add the items
    for(const auto& item : action.ExtraRemovedGroupImages)
        group.push_back(item);
    for(const auto& item : action.RemovedImages)
        group.push_back(item);

    _BrowseDuplicates(action.StoredShownDuplicateGroup);
    return true;
}
// ------------------------------------ //
void DuplicateFinderWindow::ResetState()
{
    // TODO: this cannot interrupt already queued background operations so the reset might not
    // "stick"

    // Reset all the status variables
    TotalGroupsFound = 0;
    FetchingNewDuplicateGroups = false;
    DuplicateGroups.clear();
    ImagesMissingSignaturesCalculated = false;
    QueryingDBForDuplicates = false;
    NoMoreQueryResults = false;
    DoneWithSignatures = false;

    // Reset all the widgets
    if(Scanning)
        _ScanButtonPressed();
    History.Clear();
    _UpdateUndoRedoButtons();
    _UpdateDuplicateWidgets();

    ProgressLabel.property_label() = PROGRESS_LABEL_INITIAL_TEXT;
    ScanProgress.property_fraction() = 0;

    MenuPopover.hide();
}
// ------------------------------------ //
void DuplicateFinderWindow::_ScanButtonPressed()
{
    constexpr auto DETECTION_STRING = "Detecting images needing signature calculation...";
    if(Scanning) {
        // Stop scanning
        Calculator.Pause();

    } else {

        if(!ImagesMissingSignaturesCalculated) {

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
                        _CheckScanStatus();
                    } else {

                        Calculator.AddImages(imagesWithoutSignature);
                        DoneWithSignatures = false;
                        ImagesMissingSignaturesCalculated = true;
                    }
                });
            });
        }

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
void DuplicateFinderWindow::_SkipPressed()
{
    if(DuplicateGroups.empty() || ShownDuplicateGroup < 0 ||
        ShownDuplicateGroup >= static_cast<int>(DuplicateGroups.size()))
        return;

    // Create a proper action out of this
    auto action = std::make_shared<HistoryItem>(
        *this, DuplicateGroups[ShownDuplicateGroup], ShownDuplicateGroup, ACTION_TYPE::Remove);

    // And put it into the history which will call Redo on it
    History.AddAction(action);

    _UpdateUndoRedoButtons();
}

void DuplicateFinderWindow::_UndoPressed()
{
    try {
        if(!History.Undo())
            throw Leviathan::Exception("unknown error in undo");

    } catch(const Leviathan::Exception& e) {
        LOG_ERROR("Undo failed:");
        e.PrintToLog();
    }

    _UpdateUndoRedoButtons();
}

void DuplicateFinderWindow::_RedoPressed()
{
    try {
        if(!History.Redo())
            throw Leviathan::Exception("unknown error in redo");

    } catch(const Leviathan::Exception& e) {
        LOG_ERROR("Redo failed:");
        e.PrintToLog();
    }

    _UpdateUndoRedoButtons();
}
// ------------------------------------ //
void DuplicateFinderWindow::_DeleteSelectedAfterFirstPressed()
{
    // Get the selected things from the super container and cast them to images
    std::vector<std::shared_ptr<ResourceWithPreview>> selected;
    DuplicateGroupImages.GetSelectedItems(selected);

    if(selected.size() < 2)
        return;

    std::vector<std::shared_ptr<Image>> imageList;

    // The first image is skipped as that's what this method is for
    for(auto iter = selected.begin() + 1; iter != selected.end(); ++iter) {

        auto casted = std::dynamic_pointer_cast<Image>(*iter);

        if(casted)
            imageList.push_back(casted);
    }

    _MergeCurrentGroupDuplicates(imageList);
}

void DuplicateFinderWindow::_DeleteAllAfterFirstPressed()
{
    auto& group = DuplicateGroups[ShownDuplicateGroup];

    _MergeCurrentGroupDuplicates(
        std::vector<std::shared_ptr<Image>>{group.begin() + 1, group.end()});
}

void DuplicateFinderWindow::_MergeCurrentGroupDuplicates(
    const std::vector<std::shared_ptr<Image>>& tomerge)
{
    if(tomerge.empty())
        return;

    std::shared_ptr<Image> mergeTarget;

    auto& group = DuplicateGroups[ShownDuplicateGroup];

    // Make sure that everything in tomerge is found
    if(std::find_if(tomerge.begin(), tomerge.end(), [&](const std::shared_ptr<Image>& image) {
           return std::find(group.begin(), group.end(), image) == group.end();
       }) != tomerge.end()) {

        LOG_ERROR("DuplicateFinder: merge list contained an image that is not part of the "
                  "current group");
        return;
    }

    // The first not selected image is the merge target
    // NOTE: this code doesn't store this detection anywhere this is just for sanity checking
    // and logging. Similar detection is in the action redo method
    for(const auto& image : group) {
        bool selected = false;

        for(const auto& check : tomerge) {
            if(check == image) {
                selected = true;
                break;
            }
        }

        if(!selected) {
            mergeTarget = image;
            break;
        }
    }

    if(!mergeTarget) {
        LOG_ERROR("No merge target detected");
        return;
    }

    LOG_INFO("Merging images into: " + mergeTarget->GetName() + " (" +
             std::to_string(mergeTarget->GetID()) + ")");

    for(const auto& image : tomerge)
        LOG_WRITE("\t" + image->GetName() + " (" + std::to_string(image->GetID()) + ")");

    // Create a proper action out of this
    auto action =
        std::make_shared<HistoryItem>(*this, tomerge, ShownDuplicateGroup, ACTION_TYPE::Merge);

    // And put it into the history which will call Redo on it
    History.AddAction(action);

    _UpdateUndoRedoButtons();
}
// ------------------------------------ //
void DuplicateFinderWindow::_NotDuplicatesPressed()
{
    if(DuplicateGroups.empty() || ShownDuplicateGroup < 0 ||
        ShownDuplicateGroup >= static_cast<int>(DuplicateGroups.size()))
        return;

    // Create a proper action out of this
    auto action = std::make_shared<HistoryItem>(*this, DuplicateGroups[ShownDuplicateGroup],
        ShownDuplicateGroup, ACTION_TYPE::NotDuplicate);

    // And put it into the history which will call Redo on it
    History.AddAction(action);

    _UpdateUndoRedoButtons();
}

void DuplicateFinderWindow::_ClearNotDuplicatesPressed()
{
    auto dialog = Gtk::MessageDialog(*this, "Clear all ignored duplicates?", false,
        Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);

    dialog.set_secondary_text(
        "If you have marked images as not duplicates in the past this action undoes all of "
        "them. It is NOT possible to undo this action.");
    int result = dialog.run();

    if(result != Gtk::RESPONSE_YES) {

        return;
    }

    set_sensitive(false);

    auto isalive = GetAliveMarker();

    DualView::Get().QueueDBThreadFunction([=]() {
        DualView::Get().GetDatabase().DeleteAllIgnorePairs();

        DualView::Get().InvokeFunction([this, isalive]() {
            INVOKE_CHECK_ALIVE_MARKER(isalive);

            set_sensitive(true);
        });
    });
}
// ------------------------------------ //
void DuplicateFinderWindow::_DetectNewDuplicates(
    const std::map<DBID, std::vector<std::tuple<DBID, int>>>& duplicates)
{
    DualView::IsOnMainThreadAssert();

    // The images are in unloaded form. So build a list of the groups to be loaded
    std::vector<std::vector<DBID>> toLoadGroups;
    toLoadGroups.reserve(duplicates.size());

    for(const auto& duplicatePair : duplicates) {

        DBID id = duplicatePair.first;
        const auto& groupTail = duplicatePair.second;

        // Skip already found
        // TODO: if this needs to be supported the logic is going to get way difficult as
        // existing groups could need things added to them
        // bool found = false;

        // for(const auto& existingGroup : DuplicateGroups) {

        //     for(auto image

        //     if(!existingGroup.empty() && existingGroup.front()->GetID() == id) {
        //         found = true;
        //         break;
        //     }
        // }
        // if(found)
        //     continue;

        // Add to the waiting load groups
        std::vector<DBID> ids{id};
        ids.reserve(1 + groupTail.size());

        for(const auto& tuple : groupTail)
            ids.push_back(std::get<0>(tuple));

        toLoadGroups.emplace_back(std::move(ids));
    }


    if(!toLoadGroups.empty()) {

        FetchingNewDuplicateGroups = true;

        auto isalive = GetAliveMarker();

        DualView::Get().QueueDBThreadFunction([=]() {
            std::vector<std::vector<std::shared_ptr<Image>>> loaded;
            loaded.reserve(toLoadGroups.size());

            {
                Database& db = DualView::Get().GetDatabase();

                GUARD_LOCK_OTHER(db);

                for(const auto& group : toLoadGroups) {

                    std::vector<std::shared_ptr<Image>> loadedGroup;
                    loadedGroup.reserve(group.size());

                    for(DBID image : group) {
                        auto loaded = db.SelectImageByID(guard, image);
                        if(loaded && !loaded->IsDeleted())
                            loadedGroup.push_back(loaded);
                    }

                    loaded.emplace_back(std::move(loadedGroup));
                }
            }

            // Sort the groups based on image size with stable sort to preserve the lower ID
            // image being first if they are equal size
            for(auto& group : loaded) {
                std::stable_sort(group.begin(), group.end(),
                    [](const std::shared_ptr<Image>& first,
                        const std::shared_ptr<Image>& second) {
                        return first->GetPixelCount() > second->GetPixelCount();
                    });
            }

            DualView::Get().InvokeFunction([this, isalive, loaded{std::move(loaded)}]() {
                INVOKE_CHECK_ALIVE_MARKER(isalive);
                TotalGroupsFound += loaded.size();

                DuplicateGroups.insert(DuplicateGroups.end(), loaded.begin(), loaded.end());

                FetchingNewDuplicateGroups = false;
                _UpdateDuplicateWidgets();
            });
        });
    }

    _UpdateDuplicateWidgets();
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
            auto duplicates = DualView::Get().GetDatabase().SelectPotentialImageDuplicates();

            DualView::Get().InvokeFunction([this, isalive, duplicates]() {
                INVOKE_CHECK_ALIVE_MARKER(isalive);

                LOG_INFO("Found " + std::to_string(duplicates.size()) +
                         " images with potential duplicates");

                QueryingDBForDuplicates = false;

                if(DoneWithSignatures) {
                    NoMoreQueryResults = true;
                    ScanProgress.property_fraction() = 1.f;
                    // Update the info label to tell the user that the search has finished
                    ProgressLabel.property_label() =
                        "Signature calculation complete. Duplicate detection is complete";
                    LOG_INFO("Final batch of duplicates read");
                }

                _DetectNewDuplicates(duplicates);
            });
        });
    }

    _UpdateDuplicateWidgets();
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

        // TODO: also periodically check during scan. Actually seems to not be possible as it
        // takes a huge amount of time for the detection to run

        if(done)
            _CheckScanStatus();
    });
}
// ------------------------------------ //
void DuplicateFinderWindow::_BrowseDuplicates(int newindex)
{
    if(newindex < 0 || newindex >= static_cast<int>(DuplicateGroups.size()))
        throw Leviathan::InvalidArgument("newindex out of range");

    ShownDuplicateGroup = newindex;

    // Set visible images
    const auto& items = DuplicateGroups[ShownDuplicateGroup];

    DuplicateGroupImages.SetShownItems(
        items.begin(), items.end(), std::make_shared<ItemSelectable>([=](ListItem& item) {
            std::vector<std::shared_ptr<ResourceWithPreview>> selected;
            DuplicateGroupImages.GetSelectedItems(selected);

            // Enable buttons //
            DeleteSelectedAfterFirst.property_sensitive() = selected.size() > 1;

            // Update the preview images
            // TODO: add image size labels for quick access to that
            if(selected.size() > 0) {
                FirstImage.SetImage(std::dynamic_pointer_cast<Image>(selected.front()));
            } else {
                FirstImage.SetImage(std::shared_ptr<Image>(nullptr));
            }

            if(selected.size() > 1) {
                LastImage.SetImage(std::dynamic_pointer_cast<Image>(selected.back()));
            } else {
                LastImage.SetImage(std::shared_ptr<Image>(nullptr));
            }
        }));

    // Select the first two images
    DuplicateGroupImages.SelectFirstItems(2);

    _UpdateDuplicateWidgets();
}

void DuplicateFinderWindow::_UpdateDuplicateWidgets()
{
    DualView::IsOnMainThreadAssert();

    std::string text;

    bool active = false;

    if(DuplicateGroups.empty()) {

        text = "No duplicates found";
        ShownDuplicateGroup = -1;

        // Reset the image view and the container
        if(!DuplicateGroupImages.IsEmpty()) {
            DuplicateGroupImages.Clear(true);
        }

    } else {

        // If no groups are selected select the first
        if(ShownDuplicateGroup == -1 ||
            ShownDuplicateGroup >= static_cast<int>(DuplicateGroups.size())) {
            // This updates the label
            _BrowseDuplicates(0);
            return;
        }

        active = true;

        text = "Resolving duplicate group " +
               std::to_string(
                   TotalGroupsFound + 1 - (DuplicateGroups.size() - ShownDuplicateGroup)) +
               " of " + std::to_string(TotalGroupsFound) + "";
    }

    // Set button state
    DeleteAllAfterFirst.property_sensitive() = active;
    NotDuplicates.property_sensitive() = active;
    Skip.property_sensitive() = active;

    if(FetchingNewDuplicateGroups)
        text += ". Fetching new duplicate images...";

    CurrentlyShownGroup.property_label() = text;
}
// ------------------------------------ //
void DuplicateFinderWindow::_UpdateUndoRedoButtons()
{
    Undo.property_sensitive() = History.CanUndo();
    Redo.property_sensitive() = History.CanRedo();
}
// ------------------------------------ //
// DuplicateFinderWindow::HistoryItem
DuplicateFinderWindow::HistoryItem::HistoryItem(DuplicateFinderWindow& target,
    const std::vector<std::shared_ptr<Image>>& removedimages,
    size_t groupsvectorindextoremoveat, ACTION_TYPE type) :
    RemovedImages(removedimages),
    GroupsVectorIndexToRemoveAt(groupsvectorindextoremoveat), Target(target), Type(type)
{}
// ------------------------------------ //
bool DuplicateFinderWindow::HistoryItem::Redo()
{
    if(IsPerformed())
        return false;

    if(!Target.PerformAction(*this))
        return false;

    Performed = true;
    return true;
}

bool DuplicateFinderWindow::HistoryItem::Undo()
{
    if(!IsPerformed())
        return false;

    if(!Target.UndoAction(*this))
        return false;

    Performed = false;
    return true;
}
// ------------------------------------ //
std::vector<std::tuple<DBID, DBID>>
    DuplicateFinderWindow::HistoryItem::GenerateIgnorePairs() const
{
    std::vector<std::tuple<DBID, DBID>> result;

    if(RemovedImages.size() < 2) {
        LOG_ERROR("DuplicateFinderWindow: HistoryItem: GenerateIgnorePairs: less than 2 "
                  "items, not generating anything");
        return result;
    }

    result.reserve(RemovedImages.size());

    auto iter = RemovedImages.begin();

    DBID first = (*iter)->GetID();
    ++iter;

    for(; iter != RemovedImages.end(); ++iter) {

        result.emplace_back(first, (*iter)->GetID());
    }

    return result;
}
