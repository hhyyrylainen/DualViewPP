// ------------------------------------ //
#include "DownloadSetup.h"

#include "resources/Folder.h"
#include "resources/InternetImage.h"
#include "resources/NetGallery.h"
#include "resources/Tags.h"

#include "components/FolderSelector.h"
#include "components/SuperContainer.h"
#include "components/SuperViewer.h"
#include "components/TagEditor.h"

#include "Database.h"
#include "DownloadManager.h"
#include "DualView.h"
#include "PluginManager.h"

#include "Common.h"

#include "Common/StringOperations.h"

using namespace DV;
// ------------------------------------ //
DownloadSetup::DownloadSetup(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));

    BUILDER_GET_WIDGET(HeaderBar);
    BUILDER_GET_WIDGET(BottomButtons);

    // No custom stuff in primary menu
    // Get and apply primary menu options
    BUILDER_GET_PRIMARY_MENU(Menu, MenuPopover);

    builder->get_widget_derived("ImageDLSelector", ImageSelection);
    LEVIATHAN_ASSERT(ImageSelection, "Invalid .glade file");

    builder->get_widget_derived("FolderSelector", TargetFolder);
    LEVIATHAN_ASSERT(TargetFolder, "Invalid .glade file");

    builder->get_widget_derived("CollectionTags", CollectionTagEditor);
    LEVIATHAN_ASSERT(CollectionTagEditor, "Invalid .glade file");

    builder->get_widget_derived(
        "CurrentImage", CurrentImage, nullptr, SuperViewer::ENABLED_EVENTS::ALL, false);

    LEVIATHAN_ASSERT(CurrentImage, "Invalid .glade file");

    builder->get_widget_derived("CurrentImageEditor", CurrentImageEditor);
    LEVIATHAN_ASSERT(CurrentImageEditor, "Invalid .glade file");

    CollectionTags = std::make_shared<TagCollection>();

    CollectionTagEditor->SetEditedTags({CollectionTags});

    BUILDER_GET_WIDGET(URLEntry);

    URLEntry->signal_activate().connect(sigc::mem_fun(*this, &DownloadSetup::OnURLChanged));

    URLEntry->signal_changed().connect(sigc::mem_fun(*this, &DownloadSetup::OnInvalidateURL));

    BUILDER_GET_WIDGET(DetectedSettings);

    BUILDER_GET_WIDGET(URLCheckSpinner);

    BUILDER_GET_WIDGET(OKButton);

    OKButton->signal_clicked().connect(
        sigc::mem_fun(*this, &DownloadSetup::OnUserAcceptSettings));

    BUILDER_GET_WIDGET(PageRangeLabel);

    BUILDER_GET_WIDGET(ScanPages);
    ScanPages->signal_clicked().connect(
        sigc::mem_fun(*this, &DownloadSetup::StartPageScanning));

    BUILDER_GET_WIDGET(PageScanSpinner);

    BUILDER_GET_WIDGET(CurrentScanURL);
    CurrentScanURL->property_label() = "";

    BUILDER_GET_WIDGET(PageScanProgress);

    BUILDER_GET_WIDGET(TargetCollectionName);
    CollectionNameCompletion.Init(TargetCollectionName, nullptr,
        std::bind(&Database::SelectCollectionNamesByWildcard, &DualView::Get().GetDatabase(),
            std::placeholders::_1, std::placeholders::_2));

    TargetCollectionName->signal_changed().connect(
        sigc::mem_fun(*this, &DownloadSetup::UpdateReadyStatus));

    BUILDER_GET_WIDGET(MainStatusLabel);

    BUILDER_GET_WIDGET(SelectOnlyOneImage);

    BUILDER_GET_WIDGET(SelectAllImagesButton);

    SelectAllImagesButton->signal_clicked().connect(
        sigc::mem_fun(*this, &DownloadSetup::SelectAllImages));

    BUILDER_GET_WIDGET(ImageSelectPageAll);
    ImageSelectPageAll->signal_clicked().connect(
        sigc::mem_fun(*this, &DownloadSetup::SelectAllImages));

    BUILDER_GET_WIDGET(DeselectImages);
    DeselectImages->signal_clicked().connect(
        sigc::mem_fun(*this, &DownloadSetup::DeselectAllImages));

    BUILDER_GET_WIDGET(BrowseForward);
    BrowseForward->signal_clicked().connect(
        sigc::mem_fun(*this, &DownloadSetup::SelectNextImage));

    BUILDER_GET_WIDGET(BrowseBack);
    BrowseBack->signal_clicked().connect(
        sigc::mem_fun(*this, &DownloadSetup::SelectPreviousImage));

    Gtk::Button* QuickSwapPages;
    BUILDER_GET_WIDGET(QuickSwapPages);
    QuickSwapPages->signal_clicked().connect(
        sigc::mem_fun(*this, &DownloadSetup::_DoQuickSwapPages));

    Gtk::Button* DetargetAndCollapse;
    BUILDER_GET_WIDGET(DetargetAndCollapse);
    DetargetAndCollapse->signal_clicked().connect(
        sigc::mem_fun(*this, &DownloadSetup::_DoDetargetAndCollapse));

    BUILDER_GET_WIDGET(SelectAllAndOK);
    SelectAllAndOK->signal_clicked().connect(
        sigc::mem_fun(*this, &DownloadSetup::_DoSelectAllAndOK));


    BUILDER_GET_WIDGET(RemoveSelected);
    RemoveSelected->signal_clicked().connect(
        sigc::mem_fun(*this, &DownloadSetup::RemoveSelectedImages));


    BUILDER_GET_WIDGET(WindowTabs);

    BUILDER_GET_WIDGET(RemoveAfterAdding);

    BUILDER_GET_WIDGET(ActiveAsAddTarget);

    // Need to override the default handler otherwise this isn't called
    ActiveAsAddTarget->signal_state_set().connect(
        sigc::mem_fun(*this, &DownloadSetup::_AddActivePressed), false);

    BUILDER_GET_WIDGET(ShowFullControls);

    // Need to override the default handler otherwise this isn't called
    ShowFullControls->signal_state_set().connect(
        sigc::mem_fun(*this, &DownloadSetup::_FullViewToggled), false);

    BUILDER_GET_WIDGET(FoundLinksBox);
    BUILDER_GET_WIDGET(CopyToClipboard);
    CopyToClipboard->signal_clicked().connect(
        sigc::mem_fun(*this, &DownloadSetup::_CopyToClipboard));

    BUILDER_GET_WIDGET(LoadFromClipboard);
    LoadFromClipboard->signal_clicked().connect(
        sigc::mem_fun(*this, &DownloadSetup::_LoadFromClipboard));

    // Set all the editor controls read only
    _UpdateWidgetStates();

    // Capture add target if none is set //
    DownloadSetup* nullValue = nullptr;
    if(IsSomeGloballyActive.compare_exchange_strong(nullValue, this)) {

        LOG_INFO("DownloadSetup automatically captured global add");
        // We now captured it
        ActiveAsAddTarget->set_active(true);
    }
}

DownloadSetup::~DownloadSetup()
{
    Close();
}

std::atomic<DownloadSetup*> DownloadSetup::IsSomeGloballyActive = {nullptr};
// ------------------------------------ //
void DownloadSetup::_OnClose()
{
    DualView::IsOnMainThreadAssert();

    // Release the global set
    if(ActiveAsAddTarget->get_active()) {

        DownloadSetup* usPtr = this;
        if(!IsSomeGloballyActive.compare_exchange_strong(usPtr, nullptr)) {

            LOG_WARNING("Our add active widget was checked, but we weren't the active ptr");
        }

        ActiveAsAddTarget->set_active(false);
    }
}
// ------------------------------------ //
void DownloadSetup::_OnFinishAccept(bool success)
{
    // Restore cursor
    auto window = get_window();
    if(window)
        window->set_cursor();

    if(!ImageObjects.empty()) {
        // Restore editing
        State = STATE::URL_OK;
        set_sensitive(true);

        ImageSelection->SetShownItems(ImageObjects.begin(), ImageObjects.end());
        UpdateEditedImages();
    }

    if(!success) {

        auto dialog = Gtk::MessageDialog(
            *this, "Error Adding Images", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);

        dialog.set_secondary_text("TODO: add the error message here");
        dialog.run();
        return;
    }

    // If there are leftover images allow adding those to another collection
    if(ImageObjects.empty()) {

        close();
        return;
    }

    // There are still some stuff //
    auto dialog = Gtk::MessageDialog(*this, "Added Some Images From This Internet Resource",
        false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true);

    dialog.set_secondary_text(
        "You can either select the remaining images and add them also. "
        "Or you can close this window to discard the rest of the images");
    dialog.run();
}

void DownloadSetup::OnUserAcceptSettings()
{
    if(State != STATE::URL_OK) {

        LOG_ERROR("DownloadSetup: trying to accept in not URL_OK state");
        return;
    }

    if(!IsReadyToDownload())
        return;

    // Make sure that the url is valid //
    SetTargetCollectionName(TargetCollectionName->get_text());

    // Ask to add to uncategorized //
    if(TargetCollectionName->get_text().empty()) {

        auto dialog = Gtk::MessageDialog(*this, "Download to Uncategorized?", false,
            Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);

        dialog.set_secondary_text("Download to Uncategorized makes finding images "
                                  "later more difficult.");
        int result = dialog.run();

        if(result != Gtk::RESPONSE_YES) {

            return;
        }
    }



    State = STATE::ADDING_TO_DB;

    // Create a DownloadCollection and add that to the database
    const auto selected = GetSelectedImages();

    // Cache all images that are already downloaded
    DualView::Get().QueueWorkerFunction([selected]() {
        for(const auto& image : selected) {

            image->SaveFileToDisk();
        }
    });

    // Store values //

    // Collection Tags
    const auto collectionTags = CollectionTags->TagsAsString(";");
    CollectionTags = std::make_shared<TagCollection>();
    CollectionTagEditor->SetEditedTags({CollectionTags});

    // Collection Path
    const auto collectionPath = TargetFolder->GetPath();
    TargetFolder->GoToRoot();

    const auto alive = GetAliveMarker();

    const bool remove = RemoveAfterAdding->get_active();

    DualView::Get().QueueWorkerFunction(
        [us{this}, alive, selected, collectionTags, collectionPath, remove,
            url{CurrentlyCheckedURL}, name{TargetCollectionName->get_text()}]() {
            auto gallery = std::make_shared<NetGallery>(url, name);

            gallery->SetTags(collectionTags);
            gallery->SetTargetPath(collectionPath);

            // Save the net gallery to the databse
            // (which also allows the DownloadManager to pick it up)
            auto& database = DualView::Get().GetDatabase();

            bool success = true;

            try {
                GUARD_LOCK_OTHER(database);

                DoDBTransaction transaction(database, guard);

                database.InsertNetGallery(guard, gallery);
                gallery->AddFilesToDownload(selected, guard);

            } catch(const DV::InvalidSQL& e) {

                LOG_ERROR("Failed to add NetGallery download: ");
                e.PrintToLog();
                success = false;
            }

            // We are done
            DualView::Get().InvokeFunction([=]() {
                INVOKE_CHECK_ALIVE_MARKER(alive);

                // Remove the added from the list //
                if(remove && success) {

                    for(size_t i = 0; i < us->ImageObjects.size();) {

                        bool removed = false;

                        for(const auto& added : selected) {
                            if(us->ImageObjects[i].get() == added.get()) {

                                us->ImageObjects.erase(us->ImageObjects.begin() + i);

                                us->ImagesToDownload.erase(us->ImagesToDownload.begin() + i);

                                removed = true;
                                break;
                            }
                        }

                        if(!removed)
                            ++i;
                    }
                }

                us->DownloadSetup::_OnFinishAccept(success);
            });
        });

    // Start waiting for things //
    set_sensitive(false);
    auto window = get_window();
    if(window)
        window->set_cursor(Gdk::Cursor::create(window->get_display(), Gdk::WATCH));
}
// ------------------------------------ //
void DownloadSetup::AddSubpage(const std::string& url, bool suppressupdate /*= false*/)
{
    for(const auto& existing : PagesToScan) {

        if(existing == url)
            return;
    }

    PagesToScan.push_back(url);

    if(!suppressupdate)
        _UpdateFoundLinks();
}

void DownloadSetup::OnFoundContent(const ScanFoundImage& content)
{
    DualView::IsOnMainThreadAssert();

    for(auto& existinglink : ImagesToDownload) {

        if(existinglink == content) {
            LOG_INFO("TODO: merge tags to the actual image object, duplicate now merged only "
                     "into ImagesToDownload links");
            existinglink.Merge(content);
            return;
        }
    }

    try {

        ImageObjects.push_back(InternetImage::Create(content, false));

    } catch(const Leviathan::InvalidArgument& e) {

        LOG_ERROR("DownloadSetup: failed to create InternetImage for link because url "
                  "is invalid, link: " +
                  content.URL + ", exception: ");
        e.PrintToLog();
        return;
    }

    // Tags //
    if(!content.Tags.empty()) {

        auto tagCollection = ImageObjects.back()->GetTags();

        LEVIATHAN_ASSERT(tagCollection, "new InternetImage has no tag collection");

        auto alive = GetAliveMarker();

        DualView::Get().QueueDBThreadFunction([=, tagStr{content.Tags}]() {
            std::vector<std::shared_ptr<AppliedTag>> parsedTags;

            for(const auto& tag : tagStr) {

                try {
                    const auto parsedtag = DualView::Get().ParseTagFromString(tag);

                    if(!parsedtag)
                        throw Leviathan::InvalidArgument("");

                    parsedTags.push_back(parsedtag);

                } catch(const Leviathan::InvalidArgument&) {

                    LOG_WARNING("DownloadSetup: unknown tag: " + tag);
                    continue;
                }
            }

            if(parsedTags.empty())
                return;

            DualView::Get().InvokeFunction([=]() {
                INVOKE_CHECK_ALIVE_MARKER(alive);

                LOG_INFO("DownloadSetup: adding found tags (" +
                         std::to_string(parsedTags.size()) + ") to image");

                for(const auto& parsedTag : parsedTags)
                    tagCollection->Add(parsedTag);
            });
        });
    }

    ImagesToDownload.push_back(content);

    // Add it to the selectable content //
    ImageSelection->AddItem(ImageObjects.back(),
        std::make_shared<ItemSelectable>(
            std::bind(&DownloadSetup::OnItemSelected, this, std::placeholders::_1)));

    LOG_INFO(
        "DownloadSetup added new image: " + content.URL + " referrer: " + content.Referrer);
}

bool DownloadSetup::IsValidTargetForImageAdd() const
{
    switch(State) {
    case STATE::URL_CHANGED:
    case STATE::URL_OK: return ActiveAsAddTarget->get_active();
    default: return false;
    }
}

void DownloadSetup::AddExternallyFoundLink(const std::string& url, const std::string& referrer)
{
    OnFoundContent(ScanFoundImage(url, referrer));

    // Update image counts and stuff //
    UpdateReadyStatus();

    if(State == STATE::URL_CHANGED)
        _SetState(STATE::URL_OK);
}

bool DownloadSetup::IsValidForNewPageScan() const
{
    if((State != STATE::URL_CHANGED && State != STATE::URL_OK) || UrlBeingChecked) {

        return false;
    }

    if(!(TargetCollectionName->get_text().empty() && URLEntry->get_text().empty())) {
        return false;
    }

    return ActiveAsAddTarget->get_active();
}

void DownloadSetup::SetNewUrlToDl(const std::string& url)
{
    URLEntry->set_text(url);
    OnURLChanged();
}

bool DownloadSetup::IsValidTargetForScanLink() const
{
    switch(State) {
    case STATE::URL_CHANGED:
    case STATE::URL_OK: return ActiveAsAddTarget->get_active();
    default: return false;
    }
}

void DownloadSetup::AddExternalScanLink(const std::string& url)
{
    DualView::IsOnMainThreadAssert();

    switch(State) {
    case STATE::URL_CHANGED:
    case STATE::URL_OK: break;
    default: return;
    }

    AddSubpage(url);

    if(State == STATE::URL_CHANGED)
        _SetState(STATE::URL_OK);

    _UpdateWidgetStates();
}

void DownloadSetup::DisableAddActive()
{
    DualView::IsOnMainThreadAssert();

    DownloadSetup* usPtr = this;
    // Doesn't matter if setting this fails
    IsSomeGloballyActive.compare_exchange_strong(usPtr, nullptr);
    ActiveAsAddTarget->set_active(false);
}

void DownloadSetup::EnableAddActive()
{
    DualView::IsOnMainThreadAssert();

    // Do we need to steal? //
    auto* other = IsSomeGloballyActive.load();
    if(other != nullptr) {

        // Steal //

        // This is where using an atomic variable breaks so we need to
        // make sure only main thread uses the atomic variable
        other->_OnActiveSlotStolen(this);
    }

    // Take it //
    DownloadSetup* nullValue = nullptr;
    if(IsSomeGloballyActive.compare_exchange_strong(nullValue, this)) {

        // We now captured it
        ActiveAsAddTarget->set_active(true);
    } else {

        // That shouldn't fail, recurse to try freeing it up again
        EnableAddActive();
    }
}

bool DownloadSetup::_AddActivePressed(bool state)
{
    // If the new state doesn't match what the add active variable
    // points to call the change methods
    const bool isUs = IsSomeGloballyActive.load() == this;

    if((isUs && state) || (!isUs && !state)) {
        // Nothing to do //
        return false;
    }

    if(state) {
        EnableAddActive();
    } else {
        DisableAddActive();
    }

    // Don't prevent default callback
    return false;
}

void DownloadSetup::_OnActiveSlotStolen(DownloadSetup* stealer)
{
    LOG_INFO("Active slot stolen from us");
    DisableAddActive();
}
// ------------------------------------ //
bool DownloadSetup::_FullViewToggled(bool state)
{
    if(state) {
        // Show everyhing //
        WindowTabs->show();
        BottomButtons->show();

        // The height is also here through trial and error
        resize(PreviousWidth - 55, PreviousHeight - 99);

    } else {

        PreviousWidth = get_allocated_width();
        PreviousHeight = get_allocated_height();

        // Hide everything //
        WindowTabs->hide();
        BottomButtons->hide();

        // Resize to minimum height //
        // No clue why this 55 is needed here, this was adjusted through trial and error
        resize(PreviousWidth - 55, 1);
    }

    return false;
}
// ------------------------------------ //
void DownloadSetup::OnItemSelected(ListItem& item)
{
    // Deselect others if only one is wanted //
    if(SelectOnlyOneImage->get_active() && item.IsSelected()) {

        // Deselect all others //
        ImageSelection->DeselectAllExcept(&item);
    }

    UpdateEditedImages();
}

void DownloadSetup::UpdateEditedImages()
{
    const auto result = GetSelectedImages();

    // Preview image //
    if(result.empty()) {

        CurrentImage->RemoveImage();

    } else {

        CurrentImage->SetImage(result.front());
    }

    // Tag editing //
    std::vector<std::shared_ptr<TagCollection>> tagstoedit;

    for(const auto& image : result) {

        tagstoedit.push_back(image->GetTags());
    }

    CurrentImageEditor->SetEditedTags(tagstoedit);

    UpdateReadyStatus();
}

std::vector<std::shared_ptr<InternetImage>> DownloadSetup::GetSelectedImages()
{
    std::vector<std::shared_ptr<InternetImage>> result;

    std::vector<std::shared_ptr<ResourceWithPreview>> SelectedItems;

    ImageSelection->GetSelectedItems(SelectedItems);

    for(const auto& preview : SelectedItems) {

        auto asImage = std::dynamic_pointer_cast<InternetImage>(preview);

        if(!asImage) {

            LOG_WARNING("DownloadSetup: SuperContainer has something that "
                        "isn't InternetImage");
            continue;
        }

        result.push_back(asImage);
    }

    return result;
}
// ------------------------------------ //
void DownloadSetup::SelectAllImages()
{
    // Fix selecting all when "select only one" is active
    const auto oldonlyone = SelectOnlyOneImage->get_active();
    SelectOnlyOneImage->set_active(false);

    ImageSelection->SelectAllItems();
    UpdateEditedImages();

    SelectOnlyOneImage->set_active(oldonlyone);
}

void DownloadSetup::DeselectAllImages()
{
    ImageSelection->DeselectAllItems();
    UpdateEditedImages();
}

void DownloadSetup::SelectNextImage()
{
    ImageSelection->SelectNextItem();
}

void DownloadSetup::SelectPreviousImage()
{
    ImageSelection->SelectPreviousItem();
}

void DownloadSetup::RemoveSelectedImages()
{
    const auto selected = GetSelectedImages();

    for(size_t i = 0; i < ImageObjects.size();) {

        bool removed = false;

        for(const auto& added : selected) {
            if(ImageObjects[i].get() == added.get()) {

                ImageObjects.erase(ImageObjects.begin() + i);

                ImagesToDownload.erase(ImagesToDownload.begin() + i);

                removed = true;
                break;
            }
        }

        if(!removed)
            ++i;
    }

    ImageSelection->SetShownItems(ImageObjects.begin(), ImageObjects.end());
    UpdateEditedImages();
}
// ------------------------------------ //
void DownloadSetup::OnURLChanged()
{
    if(UrlBeingChecked)
        return;

    UrlBeingChecked = true;
    _SetState(STATE::CHECKING_URL);

    DetectedSettings->set_text("Checking for valid URL, please wait.");

    std::string str = URLEntry->get_text();
    CurrentlyCheckedURL = str;

    // Find plugin for URL //
    auto scanner = DualView::Get().GetPluginManager().GetScannerForURL(str);

    if(!scanner) {

        UrlCheckFinished(false, "No plugin found that supports input url");
        return;
    }

    // Link rewrite //
    if(scanner->UsesURLRewrite()) {

        str = scanner->RewriteURL(str);
        URLEntry->set_text(str.c_str());
    }

    // Detect single image page
    bool singleImagePage = false;

    if(scanner->IsUrlNotGallery(str)) {

        singleImagePage = true;
    }

    try {

        auto scan = std::make_shared<PageScanJob>(str, true);

        auto alive = GetAliveMarker();

        scan->SetFinishCallback([this, alive, weakScan = std::weak_ptr<PageScanJob>(scan), str,
                                    singleImagePage](DownloadJob& job, bool success) {
            auto scan = weakScan.lock();

            DualView::Get().InvokeFunction([=]() {
                INVOKE_CHECK_ALIVE_MARKER(alive);

                if(!scan) {

                    LOG_ERROR("Scan object is dead, failing scan");
                    UrlCheckFinished(false, "URL scanning failed (scan object is dead)");
                    return;
                }

                // Store the pages //
                if(success) {

                    ScanResult& result = scan->GetResult();

                    // Add the main page //
                    AddSubpage(str, true);

                    for(const auto& page : result.PageLinks)
                        AddSubpage(page, true);

                    _UpdateFoundLinks();

                    // Set the title //
                    if(!result.PageTitle.empty())
                        SetTargetCollectionName(result.PageTitle);
                }


                // Finished //
                if(!success) {

                    UrlCheckFinished(false, "URL scanning failed");
                    return;
                }

                // Set tags //
                ScanResult& result = scan->GetResult();
                if(!result.PageTags.empty() && !singleImagePage) {

                    LOG_INFO("DownloadSetup parsing tags, count: " +
                             Convert::ToString(result.PageTags.size()));

                    for(const auto& ptag : result.PageTags) {

                        try {

                            const auto tag = DualView::Get().ParseTagFromString(ptag);

                            if(!tag)
                                throw Leviathan::InvalidArgument("");

                            CollectionTags->Add(tag);

                        } catch(const Leviathan::InvalidArgument&) {

                            LOG_WARNING("DownloadSetup: unknown tag: " + ptag);
                            continue;
                        }
                    }
                }

                // Force rereading properties //
                CollectionTagEditor->ReadSetTags();

                DetectedSettings->set_text("All Good");
                UrlCheckFinished(true, "");
            });
        });

        DualView::Get().GetDownloadManager().QueueDownload(scan);

    } catch(const Leviathan::InvalidArgument&) {

        // Invalid url //
        UrlCheckFinished(false, "website not supported");
    }

    UrlBeingChecked = false;
}

void DownloadSetup::OnInvalidateURL()
{
    // This gets called if an url rewrite happens in OnURLChanged
    if(UrlBeingChecked)
        return;

    // Don't invalidate if empty //
    if(URLEntry->get_text().empty()) {

        // Enable editing if content has been found already //
        if(!ImagesToDownload.empty())
            _SetState(STATE::URL_OK);
        return;
    }

    _SetState(STATE::URL_CHANGED);
    DetectedSettings->set_text("URL changed, accept it to update.");
}

void DownloadSetup::UrlCheckFinished(bool wasvalid, const std::string& message)
{
    DualView::IsOnMainThreadAssert();

    UrlBeingChecked = false;

    if(!wasvalid) {

        DetectedSettings->set_text("Invalid URL: " + message);

        // If we already have images then we shouldn't lock stuff
        if(PagesToScan.empty() && ImagesToDownload.empty())
            _SetState(STATE::URL_CHANGED);
        return;
    }

    // The scanner settings are updated when the state is set to STATE::URL_OK automatically //
    _SetState(STATE::URL_OK);
}
// ------------------------------------ //
//! Data for DownloadSetup::StartPageScanning
struct DV::SetupScanQueueData {

    std::string MainReferrer;

    std::vector<std::string> PagesToScan;
    size_t CurrentPageToScan = 0;

    ScanResult Scans;
};

void DV::QueueNextThing(std::shared_ptr<SetupScanQueueData> data, DownloadSetup* setup,
    IsAlive::AliveMarkerT alive, std::shared_ptr<PageScanJob> scanned)
{
    if(scanned) {

        data->Scans.Combine(scanned->GetResult());

        // If found new subpages add them to the queue to scan them now too //
        for(const auto& subpage : scanned->GetResult().PageLinks) {

            // Skip duplicates //
            bool found = false;
            for(const auto& existing : data->PagesToScan) {

                if(existing == subpage) {

                    found = true;
                    break;
                }
            }

            LOG_INFO("DownloadSetup: found subpage, adding to queue to scan all in "
                     "one go: " +
                     subpage);

            if(!found) {
                data->PagesToScan.push_back(subpage);
            }
        }
    }

    auto finished = [=]() {
        DualView::IsOnMainThreadAssert();
        INVOKE_CHECK_ALIVE_MARKER(alive);

        LOG_INFO("Finished Scanning");

        // Add the content //
        for(const auto& content : data->Scans.ContentLinks)
            setup->OnFoundContent(content);

        // Add new subpages //
        for(const auto& page : data->Scans.PageLinks)
            setup->AddSubpage(page, true);

        setup->_UpdateFoundLinks();

        setup->PageScanProgress->set_value(1.0);
        setup->_SetState(DownloadSetup::STATE::URL_OK);
    };

    if(data->PagesToScan.size() <= data->CurrentPageToScan) {

        LOG_INFO("DownloadSetup scan finished, result:");
        data->Scans.PrintInfo();
        DualView::Get().InvokeFunction(finished);
        return;
    }

    LOG_INFO("DownloadSetup running scanning task " +
             Convert::ToString(data->CurrentPageToScan + 1) + "/" +
             Convert::ToString(data->PagesToScan.size()));


    float progress = static_cast<float>(data->CurrentPageToScan) / data->PagesToScan.size();

    const auto str = data->PagesToScan[data->CurrentPageToScan];
    ++data->CurrentPageToScan;

    // Update status //
    DualView::Get().InvokeFunction([setup, alive, str, progress]() {
        INVOKE_CHECK_ALIVE_MARKER(alive);

        // Scanned link //
        setup->CurrentScanURL->set_uri(str);
        setup->CurrentScanURL->set_label(str);
        setup->CurrentScanURL->set_sensitive(true);

        // Progress bar //
        setup->PageScanProgress->set_value(progress);
    });


    try {
        auto scan = std::make_shared<PageScanJob>(str, false, data->MainReferrer);

        // Queue next call //
        scan->SetFinishCallback(
            [=, weakScan = std::weak_ptr<PageScanJob>(scan)](DownloadJob& job, bool result) {
                DualView::Get().InvokeFunction([=]() {
                    INVOKE_CHECK_ALIVE_MARKER(alive);

                    if(setup->State != DownloadSetup::STATE::SCANNING_PAGES) {
                        LOG_INFO("DownloadSetup: scan cancelled");
                        return;
                    }

                    DualView::Get().QueueWorkerFunction(
                        std::bind(&DV::QueueNextThing, data, setup, alive, weakScan.lock()));
                });
            });

        DualView::Get().GetDownloadManager().QueueDownload(scan);

    } catch(const Leviathan::InvalidArgument&) {

        LOG_ERROR("DownloadSetup invalid url to scan: " + str);
    }
}

void DownloadSetup::StartPageScanning()
{
    DualView::IsOnMainThreadAssert();

    auto expectedstate = STATE::URL_OK;
    if(!State.compare_exchange_weak(expectedstate, STATE::SCANNING_PAGES,
           std::memory_order_release, std::memory_order_acquire)) {
        LOG_ERROR("Tried to enter DownloadSetup::StartPageScanning while not in URL_OK state");
        return;
    }

    _UpdateWidgetStates();

    auto alive = GetAliveMarker();

    auto data = std::make_shared<SetupScanQueueData>();
    data->PagesToScan = PagesToScan;
    data->MainReferrer = CurrentlyCheckedURL;

    DualView::Get().QueueWorkerFunction(
        std::bind(&QueueNextThing, data, this, alive, nullptr));
}

// ------------------------------------ //
void DownloadSetup::SetTargetCollectionName(const std::string& str)
{
    TargetCollectionName->set_text(
        Leviathan::StringOperations::ReplaceSingleCharacter<std::string>(str, "/\\", ' '));
}
// ------------------------------------ //
void DownloadSetup::_SetState(STATE newstate)
{
    if(State == newstate)
        return;

    State = newstate;
    auto alive = GetAliveMarker();

    DualView::Get().RunOnMainThread([this, alive]() {
        INVOKE_CHECK_ALIVE_MARKER(alive);

        _UpdateWidgetStates();
    });
}

void DownloadSetup::_UpdateWidgetStates()
{
    DualView::IsOnMainThreadAssert();

    // Spinners //
    URLCheckSpinner->property_active() = State == STATE::CHECKING_URL;
    PageScanSpinner->property_active() = State == STATE::SCANNING_PAGES;

    // Set button states //
    ScanPages->set_sensitive(State == STATE::URL_OK);

    if(State == STATE::URL_OK) {

        TargetFolder->set_sensitive(true);
        CollectionTagEditor->set_sensitive(true);
        CurrentImageEditor->set_sensitive(true);
        CurrentImage->set_sensitive(true);
        OKButton->set_sensitive(true);
        SelectAllAndOK->set_sensitive(true);
        ImageSelection->set_sensitive(true);
        TargetCollectionName->set_sensitive(true);
        SelectAllImagesButton->set_sensitive(true);
        DeselectImages->set_sensitive(true);
        ImageSelectPageAll->set_sensitive(true);
        BrowseForward->set_sensitive(true);
        BrowseBack->set_sensitive(true);
        RemoveSelected->set_sensitive(true);

    } else {

        // We want to be able to change the folder and edit tags while scanning //
        if(State != STATE::SCANNING_PAGES) {

            TargetFolder->set_sensitive(false);
            CollectionTagEditor->set_sensitive(false);
        }

        CurrentImageEditor->set_sensitive(false);
        CurrentImage->set_sensitive(false);
        OKButton->set_sensitive(false);
        SelectAllAndOK->set_sensitive(false);
        ImageSelection->set_sensitive(false);
        TargetCollectionName->set_sensitive(false);
        SelectAllImagesButton->set_sensitive(false);
        DeselectImages->set_sensitive(false);
        ImageSelectPageAll->set_sensitive(false);
        BrowseForward->set_sensitive(false);
        BrowseBack->set_sensitive(false);
        RemoveSelected->set_sensitive(false);
    }

    LoadFromClipboard->set_sensitive(State != STATE::SCANNING_PAGES);

    switch(State) {
    case STATE::CHECKING_URL: {

    } break;
    case STATE::URL_CHANGED: {


    } break;
    case STATE::URL_OK: {
        // Update page scan state //
        if(PagesToScan.empty()) {

            PageRangeLabel->set_text("0");

        } else {

            PageRangeLabel->set_text("1-" + Convert::ToString(PagesToScan.size()));
        }

        _UpdateFoundLinks();

        // Update main status //
        UpdateReadyStatus();

    } break;
    case STATE::SCANNING_PAGES: {


    } break;
    case STATE::ADDING_TO_DB: {

    } break;
    }
}

void DownloadSetup::UpdateReadyStatus()
{
    const auto selected = ImageSelection->CountSelectedItems();
    const auto total = ImageObjects.size();

    bool ready = IsReadyToDownload();

    MainStatusLabel->set_text(
        (ready ? "Ready" : "Not ready") +
        std::string(" to download " + Convert::ToString(selected) + " (out of " +
                    Convert::ToString(total) + ") images to \"" +
                    TargetCollectionName->get_text() + "\""));
}

bool DownloadSetup::IsReadyToDownload() const
{
    if(State != STATE::URL_OK)
        return false;

    const auto selected = ImageSelection->CountSelectedItems();
    const auto total = ImageObjects.size();

    return selected > 0 && selected <= total;
}
// ------------------------------------ //
void DownloadSetup::_DoQuickSwapPages()
{
    if(WindowTabs->get_current_page() == 0) {

        WindowTabs->set_current_page(1);

    } else {

        WindowTabs->set_current_page(0);
    }
}

void DownloadSetup::_DoDetargetAndCollapse()
{
    _AddActivePressed(false);
    ShowFullControls->property_state() = false;
}

void DownloadSetup::_DoSelectAllAndOK()
{
    SelectAllImages();
    OnUserAcceptSettings();
}

// ------------------------------------ //
void DownloadSetup::_UpdateFoundLinks()
{
    if(!DualView::Get().IsOnMainThread()) {

        const auto alive = GetAliveMarker();
        DualView::Get().InvokeFunction([=]() {
            INVOKE_CHECK_ALIVE_MARKER(alive);

            this->_UpdateFoundLinks();
        });

        return;
    }

    std::vector<std::string> existingLinks;
    const auto children = FoundLinksBox->get_children();
    existingLinks.reserve(children.size());


    for(Gtk::Widget* child : children) {
        const Glib::ustring uri = static_cast<const Gtk::LinkButton*>(child)->property_uri();

        bool good = false;

        for(const auto& page : PagesToScan) {
            if(page == uri) {
                good = true;
                break;
            }
        }

        if(good) {
            existingLinks.push_back(uri);

        } else {
            FoundLinksBox->remove(*child);
            delete child;
        }
    }

    for(const auto& page : PagesToScan) {

        bool exists = false;

        for(const auto& existing : existingLinks) {
            if(existing == page) {
                exists = true;
                break;
            }
        }

        if(!exists) {

            Glib::ustring uri = page;

            auto* button =
                Glib::wrap(reinterpret_cast<GtkLinkButton*>(gtk_link_button_new(uri.c_str())));


            // Doesn't work with my version of gtk (linking fails)
            // Gtk::LinkButton* button = Gtk::make_managed<Gtk::LinkButton>(page);
            // Linking fails with this
            // FoundLinksBox->add(*Gtk::manage(new Gtk::LinkButton(page)));
            FoundLinksBox->add(*Gtk::manage(button));
            button->show();
        }
    }
}
// ------------------------------------ //
void DownloadSetup::_CopyToClipboard()
{
    // TODO: maybe having them as URIs would be better
    std::stringstream text;

    bool first = true;
    for(const auto& page : PagesToScan) {
        if(!first)
            text << "\n";
        first = false;
        text << page;
    }

    Gtk::Clipboard::get()->set_text(text.str());
}

void DownloadSetup::_LoadFromClipboard()
{
    const std::string text = Gtk::Clipboard::get()->wait_for_text();

    if(text.empty()) {
        LOG_INFO("Clipboard is empty or has no text");
        return;
    }
    std::vector<std::string> lines;
    Leviathan::StringOperations::CutLines(text, lines);

    for(auto& line : lines) {

        Leviathan::StringOperations::RemovePreceedingTrailingSpaces(line);

        if(line.empty())
            continue;

        LOG_INFO("Adding URL: " + line);
        AddSubpage(line, true);
    }

    _UpdateFoundLinks();

    // Update image counts and stuff //
    UpdateReadyStatus();

    if(State == STATE::URL_CHANGED)
        _SetState(STATE::URL_OK);
}
