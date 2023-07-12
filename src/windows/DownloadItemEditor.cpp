// ------------------------------------ //
#include "DownloadItemEditor.h"

#include "resources/InternetImage.h"
#include "resources/NetGallery.h"
#include "windows/DownloadSetup.h"

#include "Database.h"
#include "DownloadManager.h"
#include "DualView.h"
#include "Exceptions.h"

using namespace DV;

// ------------------------------------ //
// DownloadItemEditor::ScanJobData
struct DownloadItemEditor::ScanJobData
{
    std::vector<ProcessableURL> PagesToScan;
    size_t CurrentPageToScan = 0;

    ScanResult Scans;
};

// ------------------------------------ //
// DownloadItemEditor
DownloadItemEditor::DownloadItemEditor(
    _GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder, const std::shared_ptr<NetGallery>& toedit) :
    Gtk::Window(window),
    EditedItem(toedit)
{
    LEVIATHAN_ASSERT(EditedItem, "EditedItem must not be null");

    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));

    BUILDER_GET_WIDGET(HeaderBar);

    // No custom stuff in primary menu
    // Get and apply primary menu options
    BUILDER_GET_PRIMARY_MENU(Menu, MenuPopover);

    BUILDER_GET_WIDGET(ScanReferrersForLinks);

    ScanReferrersForLinks->signal_clicked().connect(
        sigc::mem_fun(*this, &DownloadItemEditor::OnStartStopReferrerScanPressed));

    BUILDER_GET_WIDGET(ReferrerScanStatus);
    BUILDER_GET_WIDGET(ReferrerScanProgress);
    BUILDER_GET_WIDGET(ReferrerScanAcceptResult);
    BUILDER_GET_WIDGET(OpenReferrersInNewSetup);

    ReferrerScanAcceptResult->signal_clicked().connect(sigc::mem_fun(*this, &DownloadItemEditor::OnAcceptNewLinks));

    OpenReferrersInNewSetup->signal_clicked().connect(
        sigc::mem_fun(*this, &DownloadItemEditor::OnOpenReferrersInNewSetup));

    LoadDownloadProperties();
}

DownloadItemEditor::~DownloadItemEditor()
{
    Close();
}

// ------------------------------------ //
void DownloadItemEditor::_OnClose()
{
    if (ScanningForFreshLinks)
    {
        StopReferrerScan();
    }
}

// ------------------------------------ //
void DownloadItemEditor::LoadDownloadProperties()
{
    auto alive = GetAliveMarker();

    DualView::Get().QueueDBThreadFunction(
        [=, gallery = EditedItem]()
        {
            auto downloads = DualView::Get().GetDatabase().SelectNetFilesFromGallery(*gallery);

            DualView::Get().InvokeFunction(
                [this, alive, downloads{std::move(downloads)}]()
                {
                    INVOKE_CHECK_ALIVE_MARKER(alive);

                    CurrentFilesForItem = std::move(downloads);

                    HeaderBar->set_subtitle("Download to " + EditedItem->GetTargetGalleryName());

                    _UpdateReferrerWidgets();
                });
        });
}

// ------------------------------------ //
void DownloadItemEditor::StartReferrerScan()
{
    if (ScanningForFreshLinks)
        return;

    ScanningForFreshLinks = true;

    DualView::IsOnMainThreadAssert();

    _UpdateReferrerWidgets();

    auto alive = GetAliveMarker();

    auto data = std::make_shared<ScanJobData>();
    std::transform(CurrentFilesForItem.begin(), CurrentFilesForItem.end(), std::back_insert_iterator(data->PagesToScan),
        [](const std::shared_ptr<NetFile>& item)
        {
            const auto scanner = DownloadSetup::GetPluginForURL(item->GetPageReferrer());

            if (scanner)
            {
                return DownloadSetup::HandleCanonization(item->GetPageReferrer(), *scanner);
            }
            else
            {
                LOG_WARNING(
                    "Missing plugin to canonize URL (referrer scan likely won't work): " + item->GetPageReferrer());
                return ProcessableURL(item->GetPageReferrer(), true);
            }
        });

    DualView::Get().QueueWorkerFunction(
        [data, this, alive] { return DownloadItemEditor::QueueNextThing(data, this, alive, nullptr); });
}

void DownloadItemEditor::StopReferrerScan()
{
    if (!ScanningForFreshLinks)
        return;

    ScanningForFreshLinks = false;
    _UpdateReferrerWidgets();
}

// ------------------------------------ //
void DownloadItemEditor::OnStartStopReferrerScanPressed()
{
    if (ScanningForFreshLinks)
    {
        StopReferrerScan();
    }
    else
    {
        StartReferrerScan();
    }
}

void DownloadItemEditor::OnAcceptNewLinks()
{
    ReferrerScanAcceptResult->property_sensitive() = false;

    auto alive = GetAliveMarker();

    DualView::Get().QueueDBThreadFunction(
        [=, gallery = EditedItem, newItems = FoundRefreshedItems]()
        {
            auto& database = DualView::Get().GetDatabase();
            std::string error = "";

            try
            {
                GUARD_LOCK_OTHER(database);

                DoDBTransaction transaction(database, guard);

                database.InsertNetGallery(guard, gallery);
                gallery->ReplaceItemsWith(newItems, guard);
            }
            catch (const Leviathan::Exception& e)
            {
                error = e.what();
                e.PrintToLog();
            }

            DualView::Get().InvokeFunction(
                [this, alive, error]()
                {
                    INVOKE_CHECK_ALIVE_MARKER(alive);

                    if (error.empty())
                    {
                        ScanReferrersForLinks->set_label("New items set successfully");
                    }
                    else
                    {
                        ScanReferrersForLinks->set_label("Failed: " + error);
                    }
                });
        });
}

// ------------------------------------ //
void DownloadItemEditor::OnOpenReferrersInNewSetup() const
{
    auto downloader = DualView::Get().OpenDownloadSetup(true);

    if (downloader)
    {
        for (const auto& item : CurrentFilesForItem)
        {
            if (!item->GetPageReferrer().empty())
            {
                downloader->AddExternalScanLinkRaw(item->GetPageReferrer());
            }
        }

        downloader->SetTargetCollectionName(EditedItem->GetTargetGalleryName());

        // TODO: target folder and tags for the collection
    }
}

// ------------------------------------ //
void DownloadItemEditor::_UpdateReferrerWidgets()
{
    if (ScanningForFreshLinks)
    {
        ScanReferrersForLinks->set_label("Stop Scanning");
        ScanReferrersForLinks->property_sensitive() = true;
        ReferrerScanStatus->set_text("Scan stopped");
    }
    else
    {
        ScanReferrersForLinks->set_label("Start Scanning Referrers");
        ScanReferrersForLinks->property_sensitive() = !CurrentFilesForItem.empty();
        ReferrerScanStatus->set_text("Scan starting");
    }

    ReferrerScanProgress->set_fraction(0);
    ReferrerScanAcceptResult->property_sensitive() = false;
    FoundRefreshedItems.clear();
}

void DownloadItemEditor::_OnReferrerScanCompleted(const ScanResult& result)
{
    // Ignore if scan was canceled
    if (!ScanningForFreshLinks)
        return;

    // Build new content
    FoundRefreshedItems.clear();

    LOG_INFO("DownloadItemEditor: _OnReferrerScanCompleted: old links to be replaced(" +
        std::to_string(CurrentFilesForItem.size()) + "):");
    for (const auto& existing : CurrentFilesForItem)
        LOG_INFO(" " + existing->GetRawURL());

    for (const auto& item : result.ContentLinks)
    {
        // Must match some existing item
        if (!item.URL.HasCanonicalURL())
            LOG_WARNING("DownloadItemEditor: _OnReferrerScanCompleted: new found link doesn't "
                        "have canonical URL set, it likely won't match correctly");

        bool found = false;

        for (const auto& existing : CurrentFilesForItem)
        {
            if (ScanFoundImage(existing->GetFileURL()) == item)
            {
                found = true;
                break;
            }
        }

        LOG_INFO("New found image link: " + item.URL.GetURL() + ", matched old: " + std::to_string(found));

        // TODO: this overwrites the tags eventually...
        if (found)
            FoundRefreshedItems.push_back(InternetImage::Create(item, true));
    }

    if (FoundRefreshedItems.size() == CurrentFilesForItem.size())
    {
        ReferrerScanStatus->set_text("Successfully found new links for all items");
    }
    else
    {
        ReferrerScanStatus->set_text("Found " + std::to_string(FoundRefreshedItems.size()) +
            " new items but there were " + std::to_string(CurrentFilesForItem.size()) + " old items");
    }

    ReferrerScanProgress->set_fraction(1.f);
    ReferrerScanAcceptResult->property_sensitive() = true;
}

// ------------------------------------ //
void DownloadItemEditor::QueueNextThing(std::shared_ptr<ScanJobData> data, DownloadItemEditor* editor,
    IsAlive::AliveMarkerT alive, std::shared_ptr<PageScanJob> scanned)
{
    if (scanned)
    {
        data->Scans.Combine(scanned->GetResult());

        // Sub page results are ignored
    }

    auto finished = [=]()
    {
        DualView::IsOnMainThreadAssert();
        INVOKE_CHECK_ALIVE_MARKER(alive);

        LOG_INFO("DownloadItemEditor: finished Scanning");
        editor->_OnReferrerScanCompleted(data->Scans);
    };

    // TODO: this is very similar to DV::QueueNextThing in DownloadSetup.h

    if (data->PagesToScan.size() <= data->CurrentPageToScan)
    {
        LOG_INFO("DownloadItemEditor: scan finished, total result:");
        data->Scans.PrintInfo();
        DualView::Get().InvokeFunction(finished);
        return;
    }

    const auto current = data->CurrentPageToScan + 1;
    const auto total = data->PagesToScan.size();

    const float progress = static_cast<float>(data->CurrentPageToScan) /  static_cast<float>(total);

    const auto url = data->PagesToScan[data->CurrentPageToScan];
    ++data->CurrentPageToScan;

    // Update status //
    DualView::Get().InvokeFunction(
        [editor, alive, url, progress, current, total]()
        {
            INVOKE_CHECK_ALIVE_MARKER(alive);

            // Scanned link //
            // setup->CurrentScanURL->set_uri(url);
            // setup->CurrentScanURL->set_label(url);
            // setup->CurrentScanURL->set_sensitive(true);

            editor->ReferrerScanStatus->set_text(
                "Scanning referrer " + std::to_string(current) + " of " + std::to_string(total));

            // Progress bar //
            editor->ReferrerScanProgress->set_fraction(progress);
        });

    try
    {
        auto scan = std::make_shared<PageScanJob>(url, false);

        // Queue next call //
        scan->SetFinishCallback(
            [=, weakScan = std::weak_ptr<PageScanJob>(scan)](DownloadJob& job, bool result)
            {
                DualView::Get().InvokeFunction(
                    [=]()
                    {
                        INVOKE_CHECK_ALIVE_MARKER(alive);

                        if (!editor->ScanningForFreshLinks)
                        {
                            LOG_INFO("DownloadItemEditor: scan cancelled");
                            return;
                        }

                        DualView::Get().QueueWorkerFunction(
                            std::bind(&DownloadItemEditor::QueueNextThing, data, editor, alive, weakScan.lock()));
                    });

                return true;
            });

        DualView::Get().GetDownloadManager().QueueDownload(scan);
    }
    catch (const Leviathan::InvalidArgument&)
    {
        LOG_ERROR("DownloadItemEditor: invalid url to scan: " + url.GetURL());
    }
}
