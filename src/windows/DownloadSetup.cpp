// ------------------------------------ //
#include "DownloadSetup.h"

#include <boost/filesystem/path.hpp>

#include "Common.h"
#include "DualView.h"

#include "Common/StringOperations.h"
#include "components/FolderSelector.h"
#include "components/SuperContainer.h"
#include "components/SuperViewer.h"
#include "components/TagEditor.h"
#include "resources/Folder.h"
#include "resources/InternetImage.h"
#include "resources/NetGallery.h"
#include "resources/Tags.h"

#include "Database.h"
#include "DownloadManager.h"
#include "FileSystem.h"
#include "PluginManager.h"
#include "Settings.h"

using namespace DV;

// ------------------------------------ //
const std::string DV::FileProtocol = "file://";

std::set<std::string> DownloadSetup::ReportedUnknownTags;
std::mutex DownloadSetup::UnknownTagMutex;

// ------------------------------------ //
DownloadSetup::DownloadSetup(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) : Gtk::Window(window)
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

    builder->get_widget_derived("CurrentImage", CurrentImage, nullptr, SuperViewer::ENABLED_EVENTS::ALL, false);

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

    OKButton->signal_clicked().connect(sigc::mem_fun(*this, &DownloadSetup::OnUserAcceptSettings));

    BUILDER_GET_WIDGET(PageRangeLabel);
    PageRangeLabel->set_text("0");

    BUILDER_GET_WIDGET(ScanPages);
    ScanPages->signal_clicked().connect(sigc::mem_fun(*this, &DownloadSetup::StartPageScanning));

    BUILDER_GET_WIDGET(PageScanSpinner);

    BUILDER_GET_WIDGET(CurrentScanURL);
    CurrentScanURL->property_label() = "";

    BUILDER_GET_WIDGET(PageScanProgress);

    BUILDER_GET_WIDGET(TargetCollectionName);
    CollectionNameCompletion.Init(TargetCollectionName, nullptr,
        [database = &DualView::Get().GetDatabase()](const std::string& str, size_t maxCount)
        { return database->SelectCollectionNamesByWildcard(str, static_cast<int64_t>(maxCount)); });

    TargetCollectionName->signal_changed().connect(sigc::mem_fun(*this, &DownloadSetup::UpdateReadyStatus));

    BUILDER_GET_WIDGET(MainStatusLabel);

    BUILDER_GET_WIDGET(SelectOnlyOneImage);

    BUILDER_GET_WIDGET(SelectAllImagesButton);

    SelectAllImagesButton->signal_clicked().connect(sigc::mem_fun(*this, &DownloadSetup::SelectAllImages));

    BUILDER_GET_WIDGET(ImageSelectPageAll);
    ImageSelectPageAll->signal_clicked().connect(sigc::mem_fun(*this, &DownloadSetup::SelectAllImages));

    BUILDER_GET_WIDGET(DeselectImages);
    DeselectImages->signal_clicked().connect(sigc::mem_fun(*this, &DownloadSetup::DeselectAllImages));

    BUILDER_GET_WIDGET(BrowseForward);
    BrowseForward->signal_clicked().connect(sigc::mem_fun(*this, &DownloadSetup::SelectNextImage));

    BUILDER_GET_WIDGET(BrowseBack);
    BrowseBack->signal_clicked().connect(sigc::mem_fun(*this, &DownloadSetup::SelectPreviousImage));

    Gtk::Button* QuickSwapPages;
    BUILDER_GET_WIDGET(QuickSwapPages);
    QuickSwapPages->signal_clicked().connect(sigc::mem_fun(*this, &DownloadSetup::_DoQuickSwapPages));

    Gtk::Button* DetargetAndCollapse;
    BUILDER_GET_WIDGET(DetargetAndCollapse);
    DetargetAndCollapse->signal_clicked().connect(sigc::mem_fun(*this, &DownloadSetup::_DoDetargetAndCollapse));

    BUILDER_GET_WIDGET(SelectAllAndOK);
    SelectAllAndOK->signal_clicked().connect(sigc::mem_fun(*this, &DownloadSetup::_DoSelectAllAndOK));

    BUILDER_GET_WIDGET(RemoveSelected);
    RemoveSelected->signal_clicked().connect(sigc::mem_fun(*this, &DownloadSetup::RemoveSelectedImages));

    BUILDER_GET_WIDGET(WindowTabs);

    BUILDER_GET_WIDGET(RemoveAfterAdding);

    BUILDER_GET_WIDGET(ActiveAsAddTarget);

    // Need to override the default handler otherwise this isn't called
    ActiveAsAddTarget->signal_state_set().connect(sigc::mem_fun(*this, &DownloadSetup::_AddActivePressed), false);

    BUILDER_GET_WIDGET(ShowFullControls);

    // Need to override the default handler otherwise this isn't called
    ShowFullControls->signal_state_set().connect(sigc::mem_fun(*this, &DownloadSetup::_FullViewToggled), false);

    // Extra settings
    BUILDER_GET_WIDGET(StoreScannedPages);
    StoreScannedPages->signal_toggled().connect(sigc::mem_fun(*this, &DownloadSetup::OnScannedPagesStoreChanged));

    BUILDER_GET_WIDGET(DumpScannedToDisk);
    DumpScannedToDisk->signal_clicked().connect(sigc::mem_fun(*this, &DownloadSetup::WriteScannedPagesToDisk));

    BUILDER_GET_WIDGET(ScanLocalFiles);
    ScanLocalFiles->signal_clicked().connect(sigc::mem_fun(*this, &DownloadSetup::LocalScanStartClicked));

    BUILDER_GET_WIDGET(SelectLocalToScan);
    SelectLocalToScan->signal_selection_changed().connect(
        sigc::mem_fun(*this, &DownloadSetup::UpdateLocalScanButtonStatus));

    BUILDER_GET_WIDGET(LocalFileScannerToUse);
    LocalFileScannerToUse->signal_changed().connect(sigc::mem_fun(*this, &DownloadSetup::UpdateLocalScanButtonStatus));

    BUILDER_GET_WIDGET(ExtraSettingsStatusText);

    // Found links tab
    BUILDER_GET_WIDGET(FoundLinksBox);
    BUILDER_GET_WIDGET(CopyToClipboard);
    CopyToClipboard->signal_clicked().connect(sigc::mem_fun(*this, &DownloadSetup::_CopyToClipboard));

    BUILDER_GET_WIDGET(LoadFromClipboard);
    LoadFromClipboard->signal_clicked().connect(sigc::mem_fun(*this, &DownloadSetup::_LoadFromClipboard));

    // Set all the editor controls read only (apply the initial state)
    _UpdateWidgetStates();

    // Capture add target if none is set //
    DownloadSetup* nullValue = nullptr;
    if (IsSomeGloballyActive.compare_exchange_strong(nullValue, this))
    {
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
    if (ActiveAsAddTarget->get_active())
    {
        DownloadSetup* usPtr = this;
        if (!IsSomeGloballyActive.compare_exchange_strong(usPtr, nullptr))
        {
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
    if (window)
    {
        window->set_cursor();
    }
    else
    {
        LOG_WARNING("DownloadSetup: missing GDK Window after finished adding downloads to the DB");
    }

    if (!ImageObjects.empty())
    {
        // Restore editing
        State = STATE::URL_OK;
        set_sensitive(true);

        ImageSelection->SetShownItems(ImageObjects.begin(), ImageObjects.end());
        UpdateEditedImages();
    }

    if (!success)
    {
        auto dialog =
            Gtk::MessageDialog(*this, "Error Adding Images", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);

        dialog.set_secondary_text("TODO: add the error message here");
        dialog.run();
        return;
    }

    // If there are leftover images allow adding those to another collection
    if (ImageObjects.empty())
    {
        close();
        return;
    }

    // There are still some stuff //
    auto dialog = Gtk::MessageDialog(
        *this, "Added Some Images From This Internet Resource", false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true);

    dialog.set_secondary_text("You can either select the remaining images and add them also. "
                              "Or you can close this window to discard the rest of the images");
    dialog.run();
}

void DownloadSetup::OnUserAcceptSettings()
{
    if (State != STATE::URL_OK)
    {
        LOG_ERROR("DownloadSetup: trying to accept in not URL_OK state");
        return;
    }

    if (!IsReadyToDownload())
        return;

    // Make sure that the url is valid //
    SetTargetCollectionName(TargetCollectionName->get_text());

    // Ask to add to uncategorized //
    if (TargetCollectionName->get_text().empty())
    {
        auto dialog = Gtk::MessageDialog(
            *this, "Download to Uncategorized?", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);

        dialog.set_secondary_text("Download to Uncategorized makes finding images "
                                  "later more difficult.");
        int result = dialog.run();

        if (result != Gtk::RESPONSE_YES)
        {
            return;
        }
    }

    State = STATE::ADDING_TO_DB;
    LOG_INFO("DownloadSetup: moving to adding to DB state");

    // Disallow user interactions while doing things
    set_sensitive(false);
    auto window = get_window();
    if (window)
        window->set_cursor(Gdk::Cursor::create(window->get_display(), Gdk::WATCH));

    // Create a DownloadCollection and add that to the database
    const auto selected = GetSelectedImages();

    // Cache all images that are already downloaded
    DualView::Get().QueueWorkerFunction(
        [selected]()
        {
            for (const auto& image : selected)
            {
                image->SaveFileToDisk();
            }
        });

    const bool remove = RemoveAfterAdding->get_active();
    const std::string name = TargetCollectionName->get_text();

    // Store values //

    // Collection Tags
    const auto collectionTags = CollectionTags->TagsAsString(";");
    CollectionTags = std::make_shared<TagCollection>();
    CollectionTagEditor->SetEditedTags({CollectionTags});

    // Collection Path
    const auto collectionPath = TargetFolder->GetPath();

    if (!TargetFolder->TargetPathLockedIn())
        TargetFolder->GoToRoot();

    const auto gallery = std::make_shared<NetGallery>(CurrentlyCheckedURL, name);
    gallery->SetTags(collectionTags);
    gallery->SetTargetPath(collectionPath);

    const auto alive = GetAliveMarker();

    LOG_INFO("Starting DownloadSetup accept in background thread...");

    // There used to be a crash when `set_sensitive(false);` was called, so this was moved to be the last thing in this
    // method, hopefully this helps against that
    DualView::Get().QueueWorkerFunction(
        [us{this}, alive, selected, gallery, remove]()
        {
            // Save the net gallery to the database (which also allows the DownloadManager to pick it up)
            auto& database = DualView::Get().GetDatabase();

            bool success = true;

            try
            {
                GUARD_LOCK_OTHER(database);

                DoDBTransaction transaction(database, guard);

                database.InsertNetGallery(guard, gallery);
                gallery->AddFilesToDownload(selected, guard);
            }
            catch (const DV::InvalidSQL& e)
            {
                LOG_ERROR("Failed to add NetGallery download: ");
                e.PrintToLog();
                success = false;
            }

            LOG_INFO("DownloadSetup: wrote net gallery (" + gallery->GetTargetGalleryName() +
                ") and files to download to the DB");

            // We are done
            DualView::Get().InvokeFunction(
                [=]()
                {
                    INVOKE_CHECK_ALIVE_MARKER(alive);

                    // Remove the added from the list //
                    if (remove && success)
                    {
                        for (size_t i = 0; i < us->ImageObjects.size();)
                        {
                            bool removed = false;

                            for (const auto& added : selected)
                            {
                                if (us->ImageObjects[i].get() == added.get())
                                {
                                    // Casts are used here to suppress a warning
                                    us->ImageObjects.erase(us->ImageObjects.begin() +
                                        static_cast<decltype(us->ImageObjects)::difference_type>(i));

                                    us->ImagesToDownload.erase(us->ImagesToDownload.begin() +
                                        static_cast<decltype(us->ImageObjects)::difference_type>(i));

                                    removed = true;
                                    break;
                                }
                            }

                            if (!removed)
                                ++i;
                        }
                    }

                    us->DownloadSetup::_OnFinishAccept(success);
                });
        });
}

// ------------------------------------ //
void DownloadSetup::AddSubpage(const ProcessableURL& url, bool suppressupdate /*= false*/)
{
    for (const auto& existing : PagesToScan)
    {
        if (existing == url)
            return;
    }

    PagesToScan.push_back(url);

    if (!suppressupdate)
        _UpdateFoundLinks();
}

void DownloadSetup::OnFoundContent(const ScanFoundImage& content)
{
    DualView::IsOnMainThreadAssert();

    for (auto& existingLink : ImagesToDownload)
    {
        if (existingLink == content)
        {
            existingLink.Merge(content);

            bool found = false;

            for (auto& existingImage : ImageObjects)
            {
                if (existingImage->MatchesFoundImage(content) && existingImage->GetTags())
                {
                    found = true;
                    AddFoundTagsToImage(existingImage->GetTags(), content.Tags);
                    break;
                }
            }

            if (!found)
            {
                LOG_ERROR("Could not merge new tags into image download setup, related image to link to not found");
            }

            return;
        }
    }

    try
    {
        ImageObjects.push_back(InternetImage::Create(content, false));
    }
    catch (const Leviathan::InvalidArgument& e)
    {
        LOG_ERROR("DownloadSetup: failed to create InternetImage for link because url is invalid, link: " +
            content.URL.GetURL() + ", exception: ");
        e.PrintToLog();
        return;
    }

    // Tags //
    if (!content.Tags.empty())
    {
        auto tagCollection = ImageObjects.back()->GetTags();

        LEVIATHAN_ASSERT(tagCollection, "new InternetImage has no tag collection");

        AddFoundTagsToImage(tagCollection, content.Tags);
    }

    ImagesToDownload.push_back(content);

    // Add it to the selectable content //
    ImageSelection->AddItem(
        ImageObjects.back(), std::make_shared<ItemSelectable>([this](ListItem& item) { OnItemSelected(item); }));

    LOG_INFO("DownloadSetup added new image: " + content.URL.GetURL() + " referrer: " + content.URL.GetReferrer());
}

bool DownloadSetup::IsValidTargetForImageAdd() const
{
    switch (State)
    {
        case STATE::URL_CHANGED:
        case STATE::URL_OK:
            return ActiveAsAddTarget->get_active();
        default:
            return false;
    }
}

void DownloadSetup::AddExternallyFoundLink(const ProcessableURL& url)
{
    OnFoundContent(ScanFoundImage(url));

    // Update image counts and stuff //
    UpdateReadyStatus();

    if (State == STATE::URL_CHANGED)
        _SetState(STATE::URL_OK);
}

void DownloadSetup::AddExternallyFoundLinkRaw(const std::string& url, const std::string& referrer)
{
    const auto scanner = GetPluginForURL(url);

    if (!scanner)
    {
        LOG_WARNING("Adding link without supported scanner, won't have canonical address set");
        AddExternallyFoundLink(ProcessableURL(url, std::string(), referrer));
    }
    else
    {
        AddExternallyFoundLink(ProcessableURL(HandleCanonization(url, *scanner), referrer));
    }
}

bool DownloadSetup::IsValidForNewPageScan() const
{
    if ((State != STATE::URL_CHANGED && State != STATE::URL_OK) || UrlBeingChecked)
    {
        return false;
    }

    if (!(TargetCollectionName->get_text().empty() && URLEntry->get_text().empty()))
    {
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
    switch (State)
    {
        case STATE::URL_CHANGED:
        case STATE::URL_OK:
            return ActiveAsAddTarget->get_active();
        default:
            return false;
    }
}

void DownloadSetup::AddExternalScanLink(const ProcessableURL& url)
{
    DualView::IsOnMainThreadAssert();

    switch (State)
    {
        case STATE::URL_CHANGED:
        case STATE::URL_OK:
            break;
        default:
            return;
    }

    AddSubpage(url);

    if (State == STATE::URL_CHANGED)
        _SetState(STATE::URL_OK);

    _UpdateWidgetStates();
}

void DownloadSetup::AddExternalScanLinkRaw(const std::string& rawURL)
{
    DualView::IsOnMainThreadAssert();

    const auto scanner = GetPluginForURL(rawURL);

    if (!scanner)
    {
        LOG_WARNING("No scanner for URL, can't add external link: " + rawURL);
        return;
    }

    AddExternalScanLink(HandleCanonization(rawURL, *scanner));
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
    if (other != nullptr)
    {
        // Steal //

        // This is where using an atomic variable breaks so we need to
        // make sure only main thread uses the atomic variable
        other->_OnActiveSlotStolen(this);
    }

    // Take it //
    DownloadSetup* nullValue = nullptr;
    if (IsSomeGloballyActive.compare_exchange_strong(nullValue, this))
    {
        // We now captured it
        ActiveAsAddTarget->set_active(true);
    }
    else
    {
        // That shouldn't fail, recurse to try freeing it up again
        EnableAddActive();
    }
}

bool DownloadSetup::_AddActivePressed(bool state)
{
    // If the new state doesn't match what the add active variable
    // points to call the change methods
    const bool isUs = IsSomeGloballyActive.load() == this;

    if ((isUs && state) || (!isUs && !state))
    {
        // Nothing to do //
        return false;
    }

    if (state)
    {
        EnableAddActive();
    }
    else
    {
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
    if (state)
    {
        // Show everyhing //
        WindowTabs->show();
        BottomButtons->show();

        // The height is also here through trial and error
        resize(PreviousWidth - 55, PreviousHeight - 99);
    }
    else
    {
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
    if (SelectOnlyOneImage->get_active() && item.IsSelected())
    {
        // Deselect all others //
        ImageSelection->DeselectAllExcept(&item);
    }

    UpdateEditedImages();
}

void DownloadSetup::UpdateEditedImages()
{
    const auto result = GetSelectedImages();

    // Preview image //
    if (result.empty())
    {
        CurrentImage->RemoveImage();
    }
    else
    {
        CurrentImage->SetImage(result.front());
    }

    // Tag editing //
    std::vector<std::shared_ptr<TagCollection>> tagstoedit;

    tagstoedit.reserve(result.size());
    for (const auto& image : result)
    {
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

    for (const auto& preview : SelectedItems)
    {
        auto asImage = std::dynamic_pointer_cast<InternetImage>(preview);

        if (!asImage)
        {
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

    for (size_t i = 0; i < ImageObjects.size();)
    {
        bool removed = false;

        for (const auto& added : selected)
        {
            if (ImageObjects[i].get() == added.get())
            {
                ImageObjects.erase(ImageObjects.begin() + i);

                ImagesToDownload.erase(ImagesToDownload.begin() + i);

                removed = true;
                break;
            }
        }

        if (!removed)
            ++i;
    }

    ImageSelection->SetShownItems(ImageObjects.begin(), ImageObjects.end());
    UpdateEditedImages();
}

// ------------------------------------ //
void DownloadSetup::OnURLChanged()
{
    if (UrlBeingChecked)
        return;

    UrlBeingChecked = true;
    _SetState(STATE::CHECKING_URL);

    DetectedSettings->set_text("Checking for valid URL, please wait.");

    std::string str = URLEntry->get_text();
    CurrentlyCheckedURL = str;

    // Find plugin for URL //
    auto scanner = GetPluginForURL(str);

    if (!scanner)
    {
        UrlCheckFinished(false, "No plugin found that supports input url");
        return;
    }

    // Link rewrite //
    if (scanner->UsesURLRewrite())
    {
        str = scanner->RewriteURL(str);
        URLEntry->set_text(str.c_str());
    }

    ProcessableURL url = HandleCanonization(str, *scanner);

    // Detect single image page
    bool singleImagePage = false;

    if (scanner->IsUrlNotGallery(url))
    {
        singleImagePage = true;
    }

    try
    {
        auto scan = std::make_shared<PageScanJob>(url, true);

        auto alive = GetAliveMarker();

        scan->SetFinishCallback(
            [this, alive, weakScan = std::weak_ptr<PageScanJob>(scan), url, singleImagePage](
                DownloadJob& job, bool success)
            {
                auto scan = weakScan.lock();

                DualView::Get().InvokeFunction(
                    [=]()
                    {
                        INVOKE_CHECK_ALIVE_MARKER(alive);

                        if (!scan)
                        {
                            LOG_ERROR("Scan object is dead, failing scan");
                            UrlCheckFinished(false, "URL scanning failed (scan object is dead)");
                            return;
                        }

                        // Store the pages //
                        if (success)
                        {
                            const auto& result = scan->GetResult();

                            // Add the main page //
                            AddSubpage(url, true);

                            for (const auto& page : result.PageLinks)
                                AddSubpage(page, true);

                            _UpdateFoundLinks();

                            // Set the title //
                            if (!result.PageTitle.empty())
                                SetTargetCollectionName(result.PageTitle);
                        }

                        // Finished //
                        if (!success)
                        {
                            UrlCheckFinished(false, "URL scanning failed");
                            return;
                        }

                        // Set tags //
                        ScanResult& result = scan->GetResult();
                        if (!result.PageTags.empty() && !singleImagePage)
                        {
                            LOG_INFO("DownloadSetup parsing tags, count: " + Convert::ToString(result.PageTags.size()));

                            for (const auto& rawTag : result.PageTags)
                            {
                                try
                                {
                                    const auto tag = DualView::Get().ParseTagFromString(rawTag);

                                    if (!tag)
                                        throw Leviathan::InvalidArgument("");

                                    CollectionTags->Add(tag);
                                }
                                catch (const Leviathan::InvalidArgument&)
                                {
                                    HandleUnknownTag(rawTag);
                                    continue;
                                }
                            }
                        }

                        // Force rereading properties //
                        CollectionTagEditor->ReadSetTags();

                        DetectedSettings->set_text("All Good");
                        UrlCheckFinished(true, "");
                    });

                return true;
            });

        DualView::Get().GetDownloadManager().QueueDownload(scan);
    }
    catch (const Leviathan::InvalidArgument&)
    {
        // Invalid url //
        UrlCheckFinished(false, "website not supported");
    }

    UrlBeingChecked = false;
}

void DownloadSetup::OnInvalidateURL()
{
    // This gets called if an url rewrite happens in OnURLChanged
    if (UrlBeingChecked)
        return;

    // Don't invalidate if empty //
    if (URLEntry->get_text().empty())
    {
        // Enable editing if content has been found already //
        if (!ImagesToDownload.empty())
            _SetState(STATE::URL_OK);
        return;
    }

    _SetState(STATE::URL_CHANGED);
    DetectedSettings->set_text("URL changed, accept it to update.");
}

void DownloadSetup::UrlCheckFinished(bool wasValid, const std::string& message)
{
    DualView::IsOnMainThreadAssert();

    UrlBeingChecked = false;

    if (!wasValid)
    {
        DetectedSettings->set_text("Invalid URL: " + message);

        // If we already have images then we shouldn't lock stuff
        if (PagesToScan.empty() && ImagesToDownload.empty())
            _SetState(STATE::URL_CHANGED);
        return;
    }

    // The scanner settings are updated when the state is set to STATE::URL_OK automatically //
    _SetState(STATE::URL_OK);
}

// ------------------------------------ //
void DownloadSetup::AddFoundTagsToImage(
    const std::shared_ptr<TagCollection>& tagDestination, const std::vector<std::string>& rawTags)
{
    LEVIATHAN_ASSERT(tagDestination, "missing tag destination");

    auto alive = GetAliveMarker();

    DualView::Get().QueueDBThreadFunction(
        [alive, tagStr = rawTags, tagDestination]()
        {
            std::vector<std::shared_ptr<AppliedTag>> parsedTags;

            for (const auto& tag : tagStr)
            {
                try
                {
                    const auto parsedTag = DualView::Get().ParseTagFromString(tag);

                    if (!parsedTag)
                        throw Leviathan::InvalidArgument("");

                    parsedTags.push_back(parsedTag);
                }
                catch (const Leviathan::InvalidArgument&)
                {
                    HandleUnknownTag(tag);
                    continue;
                }
            }

            if (parsedTags.empty())
                return;

            DualView::Get().InvokeFunction(
                [alive, tagDestination, parsedTags]()
                {
                    INVOKE_CHECK_ALIVE_MARKER(alive);

                    LOG_INFO("DownloadSetup: adding found tags (" + std::to_string(parsedTags.size()) + ") to image");

                    for (const auto& parsedTag : parsedTags)
                        tagDestination->Add(parsedTag);
                });
        });
}

// ------------------------------------ //
//! Data for DownloadSetup::StartPageScanning
struct DV::SetupScanQueueData
{
    std::string MainReferrer;

    std::vector<ProcessableURL> PagesToScan;
    size_t CurrentPageToScan = 0;

    //! If not empty, overrides the URL used to detect the scan plugin
    std::string OverridePluginUrl;

    ScanResult Scans;

    //! Lock always when handling the data here
    Mutex ResultMutex;
};

bool DV::QueueNextThing(const std::shared_ptr<SetupScanQueueData>& data, DownloadSetup* setup,
    const IsAlive::AliveMarkerT& alive, const std::shared_ptr<PageScanJob>& scanned)
{
    if (scanned)
    {
        Lock lock(data->ResultMutex);

        bool foundContent = false;

        const auto combineResult = data->Scans.Combine(scanned->GetResult());

        if (combineResult & ResultCombine::NewContent)
        {
            foundContent = true;
        }

        // If found new subpages, add them to the queue to scan them now too //
        for (const auto& subpage : scanned->GetResult().PageLinks)
        {
            // Skip duplicates //
            bool found = false;
            for (const auto& existing : data->PagesToScan)
            {
                if (existing == subpage)
                {
                    found = true;
                    break;
                }
            }

            LOG_INFO("DownloadSetup: found subpage, adding to queue to scan all in one go: " + subpage.GetURL());

            if (!found)
            {
                data->PagesToScan.push_back(subpage);
                foundContent = true;
            }
        }

        if (!foundContent)
        {
            LOG_INFO("DownloadSetup: page scan found no new stuff, retrying scanning: " + scanned->GetURL().GetURL());
            return false;
        }
    }
    else
    {
        if (data->CurrentPageToScan > 0)
        {
            LOG_ERROR("Scan result is missing even though this isn't the first call, "
                      "it should exist to not lose data");
        }
    }

    auto finished = [=]()
    {
        DualView::IsOnMainThreadAssert();
        INVOKE_CHECK_ALIVE_MARKER(alive);

        Lock lock(data->ResultMutex);
        LOG_INFO("Finished Scanning");

        // Add the content //
        for (const auto& content : data->Scans.ContentLinks)
            setup->OnFoundContent(content);

        // Add new subpages //
        for (const auto& page : data->Scans.PageLinks)
            setup->AddSubpage(page, true);

        setup->_UpdateFoundLinks();

        setup->PageScanProgress->set_value(1.0);
        setup->_SetState(DownloadSetup::STATE::URL_OK);

        setup->UpdateCanWritePagesStatus();

        if (setup->ImageObjects.size() < data->Scans.ContentLinks.size())
        {
            std::string message = "DownloadSetup: less image objects created than found content links (";
            message += std::to_string(setup->ImageObjects.size());
            message += " < (scanned) ";
            message += std::to_string(data->Scans.ContentLinks.size());
            LOG_WARNING(message);
        }
    };

    if (data->PagesToScan.size() <= data->CurrentPageToScan)
    {
        Lock lock(data->ResultMutex);

        LOG_INFO("DownloadSetup scan finished, result:");
        data->Scans.PrintInfo();

        DualView::Get().InvokeFunction(finished);
        return true;
    }

    LOG_INFO("DownloadSetup running scanning task " + Convert::ToString(data->CurrentPageToScan + 1) + "/" +
        Convert::ToString(data->PagesToScan.size()));

    float progress = static_cast<float>(data->CurrentPageToScan) / static_cast<float>(data->PagesToScan.size());

    const auto url = data->PagesToScan[data->CurrentPageToScan];
    ++data->CurrentPageToScan;

    // Update status //
    DualView::Get().InvokeFunction(
        [setup, alive, url, progress]()
        {
            INVOKE_CHECK_ALIVE_MARKER(alive);

            // Scanned link //
            setup->CurrentScanURL->set_uri(url.GetURL());
            setup->CurrentScanURL->set_label(url.GetURL());
            setup->CurrentScanURL->set_sensitive(true);

            // Progress bar //
            setup->PageScanProgress->set_value(progress);
        });

    try
    {
        // Set the right referrer
        const auto withMainUrl =
            (data->CurrentPageToScan < 2 || url.GetReferrer().empty()) && data->MainReferrer.empty() ?
            url :
            ProcessableURL(url, data->MainReferrer);

        std::shared_ptr<PageScanJob> scan;

        // Locally cached file handling
        if (withMainUrl.GetURL().find(FileProtocol) == 0)
        {
            scan = std::make_shared<CachedPageScanJob>(
                withMainUrl.GetURL().substr(FileProtocol.size()), ProcessableURL(data->OverridePluginUrl, true));
        }
        else
        {
            scan = std::make_shared<PageScanJob>(withMainUrl, false);
        }

        // Queue next call //
        scan->SetFinishCallback(
            [=](DownloadJob& job, bool result)
            {
                {
                    Lock lock(setup->ScannedPageContentMutex);

                    if (setup->SaveScannedPageContent)
                    {
                        if (result)
                        {
                            LOG_INFO("Saving content of scanned page in memory: " + job.GetURL().GetURL());
                            setup->ScannedPageContent[job.GetURL().GetURL()] = job.GetDownloadedBytes();
                        }
                        else
                        {
                            LOG_INFO(
                                "Failed to download page, can't save its content in memory" + job.GetURL().GetURL());

                            const auto existing = setup->ScannedPageContent.find(job.GetURL().GetURL());

                            if (existing != setup->ScannedPageContent.end())
                                setup->ScannedPageContent.erase(existing);
                        }
                    }
                }

                DualView::Get().InvokeFunction(
                    [=]()
                    {
                        INVOKE_CHECK_ALIVE_MARKER(alive);

                        if (setup->State != DownloadSetup::STATE::SCANNING_PAGES)
                        {
                            LOG_INFO("DownloadSetup: scan cancelled");
                            return;
                        }

                        DualView::Get().QueueWorkerFunction(
                            [data, setup, alive, scan] { return DV::QueueNextThing(data, setup, alive, scan); });
                    });

                return true;
            });

        DualView::Get().GetDownloadManager().QueueDownload(scan);
    }
    catch (const Leviathan::InvalidArgument&)
    {
        LOG_ERROR("DownloadSetup invalid url to scan: " + url.GetURL());
    }

    return true;
}

void DownloadSetup::StartPageScanning()
{
    DualView::IsOnMainThreadAssert();

    auto expectedState = STATE::URL_OK;
    if (!State.compare_exchange_weak(
            expectedState, STATE::SCANNING_PAGES, std::memory_order_release, std::memory_order_acquire))
    {
        LOG_ERROR("Tried to enter DownloadSetup::StartPageScanning while not in URL_OK state");
        return;
    }

    // Clear previous scan stored temporary data (if any)
    {
        Lock lock(ScannedPageContentMutex);
        ScannedPageContent.clear();
    }

    _UpdateWidgetStates();

    auto alive = GetAliveMarker();

    auto data = std::make_shared<SetupScanQueueData>();
    data->PagesToScan = PagesToScan;
    data->MainReferrer = CurrentlyCheckedURL;

    DualView::Get().QueueWorkerFunction([data, this, alive] { return QueueNextThing(data, this, alive, nullptr); });
}

void DownloadSetup::StartLocalFileScanning(const std::string& folder, const std::string& urlForScannerSelection)
{
    DualView::IsOnMainThreadAssert();

    auto currentState = State.load(std::memory_order_acquire);

    if (currentState == STATE::SCANNING_PAGES || currentState == STATE::CHECKING_URL ||
        currentState == STATE::ADDING_TO_DB)
    {
        LOG_ERROR("Can't start local scan due to bad current state");
        return;
    }

    if (!State.compare_exchange_weak(
            currentState, STATE::SCANNING_PAGES, std::memory_order_release, std::memory_order_acquire))
    {
        LOG_ERROR("StartLocalFileScanning start failed because State variable was modified by someone else");
        return;
    }

    // Clear previous scan stored temporary data (if any)
    {
        Lock lock(ScannedPageContentMutex);
        ScannedPageContent.clear();
    }

    auto alive = GetAliveMarker();

    auto data = std::make_shared<SetupScanQueueData>();

    data->PagesToScan;

    for (boost::filesystem::directory_iterator iter(folder); iter != boost::filesystem::directory_iterator(); ++iter)
    {
        if (boost::filesystem::is_directory(iter->status()))
            continue;

        const auto path = boost::filesystem::absolute(*iter).string();

        // Only scan things that probably have html in them
        if (path.find(".html") != std::string::npos)
            data->PagesToScan.emplace_back("file://" + path, true);
    }

    data->OverridePluginUrl = urlForScannerSelection;

    DualView::Get().QueueWorkerFunction([data, this, alive] { return QueueNextThing(data, this, alive, nullptr); });
}

// ------------------------------------ //
void DownloadSetup::SetTargetCollectionName(const std::string& str)
{
    auto sanitized = Leviathan::StringOperations::ReplaceSingleCharacter<std::string>(str, '/', ' ');
    sanitized = Leviathan::StringOperations::ReplaceSingleCharacter<std::string>(str, '\\', ' ');

    TargetCollectionName->set_text(sanitized);
}

// ------------------------------------ //
void DownloadSetup::_SetState(STATE newstate)
{
    if (State == newstate)
        return;

    State = newstate;
    auto alive = GetAliveMarker();

    DualView::Get().RunOnMainThread(
        [this, alive]()
        {
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

    if (State == STATE::URL_OK)
    {
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
    }
    else
    {
        // We want to be able to change the folder and edit tags while scanning //
        if (State != STATE::SCANNING_PAGES)
        {
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

    switch (State)
    {
        case STATE::CHECKING_URL:
        case STATE::URL_CHANGED:
            break;
        case STATE::URL_OK:
        {
            // Update page scan state //
            if (PagesToScan.empty())
            {
                PageRangeLabel->set_text("0");
            }
            else
            {
                PageRangeLabel->set_text("1-" + Convert::ToString(PagesToScan.size()));
            }

            _UpdateFoundLinks();

            // Update main status //
            UpdateReadyStatus();

            break;
        }

        case STATE::SCANNING_PAGES:
        case STATE::ADDING_TO_DB:
            break;
    }

    UpdateLocalScanButtonStatus();
}

void DownloadSetup::UpdateReadyStatus()
{
    const auto selected = ImageSelection->CountSelectedItems();
    const auto total = ImageObjects.size();

    bool ready = IsReadyToDownload();

    MainStatusLabel->set_text((ready ? "Ready" : "Not ready") +
        std::string(" to download " + Convert::ToString(selected) + " (out of " + Convert::ToString(total) +
            ") images to \"" + TargetCollectionName->get_text() + "\""));
}

bool DownloadSetup::IsReadyToDownload() const
{
    if (State != STATE::URL_OK)
        return false;

    const auto selected = ImageSelection->CountSelectedItems();
    const auto total = ImageObjects.size();

    return selected > 0 && selected <= total;
}

// ------------------------------------ //
void DownloadSetup::WriteScannedPagesToDisk()
{
    DualView::IsOnMainThreadAssert();

    {
        Lock lock(ScannedPageContentMutex);

        if (ScannedPageContent.empty())
        {
            ExtraSettingsStatusText->set_text("No pages stored in memory, please perform a scan first");
            return;
        }
    }

    ExtraSettingsStatusText->set_text("Writing in memory stored pages to disk...");

    const auto alive = GetAliveMarker();

    DualView::Get().QueueWorkerFunction(
        [this, alive]()
        {
            const auto targetFolder =
                boost::filesystem::path(DualView::Get().GetSettings().GetStagingFolder()) / "scanned_pages";

            Lock lock(ScannedPageContentMutex);

            try
            {
                if (boost::filesystem::exists(targetFolder))
                {
                    boost::filesystem::remove_all(targetFolder);
                }

                boost::filesystem::create_directories(targetFolder);
            }
            catch (const std::exception& e)
            {
                DualView::Get().InvokeFunction(
                    [=]()
                    {
                        INVOKE_CHECK_ALIVE_MARKER(alive);

                        LOG_ERROR("Failed to prepare " + targetFolder.string() + " with exception: " + e.what());
                        ExtraSettingsStatusText->set_text("Error preparing the folder to write the pages");
                    });

                return;
            }

            try
            {
                for (const auto& [page, content] : ScannedPageContent)
                {
                    // Some sanitization on the URL to convert it to a valid local filename
                    std::string sanitized = page;
                    for (char invalid : "\\/<>:\"|")
                    {
                        sanitized =
                            Leviathan::StringOperations::ReplaceSingleCharacter<std::string>(sanitized, invalid, '-');
                    }

                    for (char& iter : sanitized)
                    {
                        if (iter <= 0x1F)
                            iter = '_';
                    }

                    // And making sure it is not overwriting anything existing nor is it too long
                    sanitized = DualView::MakePathUniqueAndShort((targetFolder / sanitized).string() + ".html");

                    auto logMessage = "Writing " + page;
                    logMessage += " to file: ";
                    logMessage += sanitized;
                    LOG_INFO(logMessage);

                    Leviathan::FileSystem::WriteToFile(content, sanitized);
                }
            }
            catch (const std::exception& e)
            {
                DualView::Get().InvokeFunction(
                    [=]()
                    {
                        INVOKE_CHECK_ALIVE_MARKER(alive);
                        ExtraSettingsStatusText->set_text(std::string("Error writing a page file: ") + e.what());
                    });

                return;
            }

            DualView::Get().InvokeFunction(
                [=]()
                {
                    INVOKE_CHECK_ALIVE_MARKER(alive);
                    ExtraSettingsStatusText->set_text("Done writing pages to: " + targetFolder.string());
                });
        });
}

// ------------------------------------ //
void DownloadSetup::_DoQuickSwapPages()
{
    if (WindowTabs->get_current_page() == 0)
    {
        WindowTabs->set_current_page(1);
    }
    else
    {
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
    if (!DualView::IsOnMainThread())
    {
        const auto alive = GetAliveMarker();
        DualView::Get().InvokeFunction(
            [=]()
            {
                INVOKE_CHECK_ALIVE_MARKER(alive);

                this->_UpdateFoundLinks();
            });

        return;
    }

    std::vector<std::string> existingLinks;
    const auto children = FoundLinksBox->get_children();
    existingLinks.reserve(children.size());

    for (Gtk::Widget* child : children)
    {
        // This only has stuff we put here meaning we know for certain what type of object is contained here
#pragma clang diagnostic push
#pragma ide diagnostic ignored "cppcoreguidelines-pro-type-static-cast-downcast"
        const Glib::ustring uri = static_cast<const Gtk::LinkButton*>(child)->property_uri();
#pragma clang diagnostic pop

        bool good = false;

        for (const auto& page : PagesToScan)
        {
            if (page.GetURL() == uri)
            {
                good = true;
                break;
            }
        }

        if (good)
        {
            existingLinks.push_back(uri);
        }
        else
        {
            FoundLinksBox->remove(*child);
            delete child;
        }
    }

    for (const auto& page : PagesToScan)
    {
        bool exists = false;

        for (const auto& existing : existingLinks)
        {
            if (existing == page.GetURL())
            {
                exists = true;
                break;
            }
        }

        if (!exists)
        {
            Glib::ustring uri = page.GetURL();

            auto* button = Glib::wrap(reinterpret_cast<GtkLinkButton*>(gtk_link_button_new(uri.c_str())));

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
void DownloadSetup::OnScannedPagesStoreChanged()
{
    DualView::IsOnMainThreadAssert();

    {
        Lock lock(ScannedPageContentMutex);
        SaveScannedPageContent = StoreScannedPages->get_active();
    }

    UpdateCanWritePagesStatus();
}

void DownloadSetup::UpdateCanWritePagesStatus()
{
    auto alive = GetAliveMarker();

    DualView::Get().RunOnMainThread(
        [this, alive]()
        {
            INVOKE_CHECK_ALIVE_MARKER(alive);

            if (State != STATE::URL_OK && State != STATE::URL_CHANGED)
            {
                DumpScannedToDisk->set_sensitive(false);
                return;
            }

            Lock lock(ScannedPageContentMutex);

            DumpScannedToDisk->set_sensitive(!ScannedPageContent.empty() && SaveScannedPageContent);
        });
}

// ------------------------------------ //
void DownloadSetup::_CopyToClipboard()
{
    // TODO: maybe having them as URIs would be better
    std::stringstream text;

    bool first = true;
    for (const auto& page : PagesToScan)
    {
        if (!first)
            text << "\n";
        first = false;
        text << page.GetURL();
    }

    Gtk::Clipboard::get()->set_text(text.str());
}

void DownloadSetup::_LoadFromClipboard()
{
    const std::string text = Gtk::Clipboard::get()->wait_for_text();

    if (text.empty())
    {
        LOG_INFO("Clipboard is empty or has no text");
        return;
    }
    std::vector<std::string> lines;
    Leviathan::StringOperations::CutLines(text, lines);

    for (auto& line : lines)
    {
        Leviathan::StringOperations::RemovePreceedingTrailingSpaces(line);

        if (line.empty())
            continue;

        LOG_INFO("Adding URL: " + line);

        const auto scanner = GetPluginForURL(line);

        if (!scanner)
        {
            LOG_WARNING("Loaded URL has no supported scanner, it won't be scanned");
        }
        else
        {
            AddSubpage(HandleCanonization(line, *scanner), true);
        }
    }

    _UpdateFoundLinks();

    // Update image counts and stuff //
    UpdateReadyStatus();

    if (State == STATE::URL_CHANGED)
        _SetState(STATE::URL_OK);
}

// ------------------------------------ //
void DownloadSetup::LocalScanStartClicked()
{
    const std::string folder = SelectLocalToScan->get_filename();
    const std::string scannerUrl = LocalFileScannerToUse->get_text();

    if (boost::filesystem::exists(folder) && !scannerUrl.empty())
    {
        StartLocalFileScanning(folder, scannerUrl);
    }
    else
    {
        LOG_ERROR("Incorrect settings to start local scan");
    }
}

void DownloadSetup::UpdateLocalScanButtonStatus()
{
    if (State == STATE::SCANNING_PAGES || State == STATE::CHECKING_URL || State == STATE::ADDING_TO_DB)
    {
        ScanLocalFiles->set_sensitive(false);
        return;
    }

    ScanLocalFiles->set_sensitive(
        !SelectLocalToScan->get_filename().empty() && !LocalFileScannerToUse->get_text().empty());
}

// ------------------------------------ //
ProcessableURL DownloadSetup::HandleCanonization(const std::string& url, IWebsiteScanner& scanner)
{
    return scanner.HasCanonicalURLFeature() ? ProcessableURL(url, scanner.ConvertToCanonicalURL(url)) :
                                              ProcessableURL(url, true);
}

std::shared_ptr<IWebsiteScanner> DownloadSetup::GetPluginForURL(const std::string& url)
{
    return DualView::Get().GetPluginManager().GetScannerForURL(url);
}

// ------------------------------------ //
void DownloadSetup::HandleUnknownTag(const std::string& tag)
{
    Lock guard(UnknownTagMutex);

    // Report each problem tag once
    if (!std::get<1>(ReportedUnknownTags.insert(tag)))
    {
        // Already there
        return;
    }

    LOG_INFO("DownloadSetup: unknown tag: " + tag);

    if (ReportedUnknownTags.size() > 10000)
    {
        LOG_WARNING("Too many unknown tags, clearing memory and reporting unknown tags again");
        ReportedUnknownTags.clear();
    }
}
