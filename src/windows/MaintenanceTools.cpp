// ------------------------------------ //
#include "MaintenanceTools.h"

#include <filesystem>
#include <utility>

#include <boost/filesystem.hpp>

#include "Common.h"
#include "DualView.h"

#include "Common/StringOperations.h"
#include "resources/Collection.h"
#include "resources/Folder.h"
#include "resources/Image.h"
#include "resources/ImagePath.h"
#include "resources/NetGallery.h"

#include "Database.h"
#include "Exceptions.h"
#include "Settings.h"

constexpr auto IMAGE_CHECK_REPORT_PROGRESS_EVERY_N = 200;
constexpr auto MAX_MAINTENANCE_RESULTS = 10000;

using namespace DV;

// ------------------------------------ //
// InvalidImagePathResultWidget
class InvalidImagePathResultWidget : public Gtk::Box,
                                     IsAlive
{
public:
    InvalidImagePathResultWidget(DBID imageId, const std::string& invalidPath, std::string autoFix) :
        Gtk::Box(Gtk::ORIENTATION_VERTICAL), TopHalf(Gtk::ORIENTATION_HORIZONTAL),
        BottomHalf(Gtk::ORIENTATION_HORIZONTAL), AutoFixButton("Apply AutoFix"), DeleteButton("Delete"),
        IgnoreButton("Ignore For Now"), ImageID(imageId), Path(invalidPath), AutoFix(std::move(autoFix))
    {
        TopHalf.pack_start(Working, false, true);
        InfoLabel.set_selectable(true);
        InfoLabel.set_line_wrap_mode(Pango::WRAP_CHAR);
        InfoLabel.set_max_width_chars(80);

        std::string text = "Image " + std::to_string(imageId) + " doesn't exist at path: " + invalidPath;

        if (HasAutoFix())
        {
            text += " Can be automatically fixed to: " + AutoFix;
        }

        InfoLabel.set_text(text);
        TopHalf.pack_end(InfoLabel, true, true);
        pack_start(TopHalf, true, true);

        DeleteButton.get_style_context()->add_class("destructive-action");

        AutoFixButton.signal_clicked().connect(sigc::mem_fun(*this, &InvalidImagePathResultWidget::_AutoFixPressed));

        if (HasAutoFix())
            BottomHalf.pack_start(AutoFixButton);

        DeleteButton.signal_clicked().connect(sigc::mem_fun(*this, &InvalidImagePathResultWidget::_DeletePressed));
        BottomHalf.pack_start(DeleteButton);

        IgnoreButton.signal_clicked().connect(sigc::mem_fun(*this, &InvalidImagePathResultWidget::_IgnorePressed));
        BottomHalf.pack_start(IgnoreButton);

        pack_end(BottomHalf, false, true);

        show_all_children();
    }

    bool HasAutoFix() const
    {
        return !AutoFix.empty();
    }

private:
    void _AutoFixPressed()
    {
        if (!HasAutoFix())
            return;

        Working.property_active() = true;
        _SetButtonStatuses(false);

        LOG_INFO("Auto fixing image: " + std::to_string(ImageID) + " path to: " + AutoFix);

        auto alive = GetAliveMarker();

        DualView::Get().QueueDBThreadFunction(
            [=, id = ImageID, fix = AutoFix]()
            {
                auto& database = DualView::Get().GetDatabase();
                GUARD_LOCK_OTHER(database);

                const auto image = database.SelectImageByID(guard, id);

                std::string message;

                if (!image)
                {
                    LOG_ERROR("Could not find image for fixing by id: " + std::to_string(ImageID));
                    message = "Failed to find target image";
                }
                else
                {
                    image->SetResourcePath(fix);
                    image->Save(database, guard);

                    message = "Successfully saved updated path";
                    LOG_INFO(message);
                }

                DualView::Get().InvokeFunction(
                    [=]()
                    {
                        INVOKE_CHECK_ALIVE_MARKER(alive);
                        Working.property_active() = false;
                        InfoLabel.set_text(message);
                    });
            });
    }

    void _DeletePressed()
    {
        Working.property_active() = true;
        _SetButtonStatuses(false);

        LOG_INFO("Deleting image with broken path: " + std::to_string(ImageID));

        auto alive = GetAliveMarker();

        DualView::Get().QueueDBThreadFunction(
            [=, id = ImageID, fix = AutoFix]()
            {
                auto& database = DualView::Get().GetDatabase();
                GUARD_LOCK_OTHER(database);

                const auto image = database.SelectImageByID(guard, id);

                std::string message;

                if (!image)
                {
                    LOG_ERROR("Could not find image for deletion by id: " + std::to_string(ImageID));
                    message = "Failed to find target image";
                }
                else if (image->IsDeleted())
                {
                    message = "Image was already deleted";
                }
                else
                {
                    database.DeleteImage(*image);

                    message = "Image deleted successfully";
                    LOG_INFO(message);
                }

                DualView::Get().InvokeFunction(
                    [=]()
                    {
                        INVOKE_CHECK_ALIVE_MARKER(alive);
                        Working.property_active() = false;
                        InfoLabel.set_text(message);
                    });
            });
    }

    void _IgnorePressed()
    {
        // We don't have a way to signal our deletion, so we just detach for now waiting to get
        // deleted later
        LOG_INFO("Ignoring image fail result for: " + std::to_string(ImageID));
        get_parent()->remove(*this);
    }

    void _SetButtonStatuses(bool sensitive)
    {
        AutoFixButton.set_sensitive(sensitive);
        DeleteButton.set_sensitive(sensitive);
        IgnoreButton.set_sensitive(sensitive);
    }

private:
    Gtk::Box TopHalf;
    Gtk::Spinner Working;
    Gtk::Label InfoLabel;

    Gtk::Box BottomHalf;
    Gtk::Button AutoFixButton;
    Gtk::Button DeleteButton;
    Gtk::Button IgnoreButton;

    const DBID ImageID;
    const std::string Path;
    const std::string AutoFix;
};

// ------------------------------------ //
// MaintenanceTools
MaintenanceTools::MaintenanceTools(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) : Gtk::Window(window)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &MaintenanceTools::_OnClose));

    signal_unmap().connect(sigc::mem_fun(*this, &MaintenanceTools::_OnHidden));
    signal_map().connect(sigc::mem_fun(*this, &MaintenanceTools::_OnShown));

    // Get and apply primary menu options
    BUILDER_GET_PRIMARY_MENU_NAMED("MenuButtonMaintenance", Menu, MenuPopover);

    BUILDER_GET_WIDGET(MaintenanceCancelButton);
    MaintenanceCancelButton->signal_clicked().connect(sigc::mem_fun(*this, &MaintenanceTools::_CancelPressed));

    BUILDER_GET_WIDGET(CheckAllExist);
    CheckAllExist->signal_clicked().connect(sigc::mem_fun(*this, &MaintenanceTools::StartImageExistCheck));

    BUILDER_GET_WIDGET(DeleteAllThumbnails);
    DeleteAllThumbnails->signal_clicked().connect(sigc::mem_fun(*this, &MaintenanceTools::StartDeleteThumbnails));

    BUILDER_GET_WIDGET(FixOrphanedResources);
    FixOrphanedResources->signal_clicked().connect(
        sigc::mem_fun(*this, &MaintenanceTools::StartDeleteOrphanedResources));

    BUILDER_GET_WIDGET(FixOrphanedImages);
    FixOrphanedImages->signal_clicked().connect(sigc::mem_fun(*this, &MaintenanceTools::StartFixOrphanedResources));

    BUILDER_GET_WIDGET(DeleteImportedStagingFolderItems);
    DeleteImportedStagingFolderItems->signal_clicked().connect(
        sigc::mem_fun(*this, &MaintenanceTools::DeleteImportedStagingFolderItemsClicked));

    BUILDER_GET_WIDGET(MaintenanceResults);
    BUILDER_GET_WIDGET(MaintenanceClearResults);
    MaintenanceClearResults->signal_clicked().connect(sigc::mem_fun(*this, &MaintenanceTools::_ClearResultsPressed));

    BUILDER_GET_WIDGET(MaintenanceSpinner);
    BUILDER_GET_WIDGET(MaintenanceStatusLabel);
    BUILDER_GET_WIDGET(MaintenanceProgressBar);
}

MaintenanceTools::~MaintenanceTools()
{
    Stop(true);
}

// ------------------------------------ //
bool MaintenanceTools::_OnClose(GdkEventAny* event)
{
    // Just hide it //
    hide();
    _OnHidden();
    return true;
}

void MaintenanceTools::_OnHidden()
{
    Stop(false);
}

void MaintenanceTools::_OnShown()
{
    _UpdateState();
}

// ------------------------------------ //
void MaintenanceTools::Stop(bool wait)
{
    RunTaskThread = false;

    if (wait && TaskRunning)
    {
        TaskThread.join();
        TaskRunning = false;
    }

    _UpdateState();
}

// ------------------------------------ //
void MaintenanceTools::StartImageExistCheck()
{
    if (RunTaskThread)
    {
        LOG_ERROR("Already doing an operation can't start a new maintenance operation");
        return;
    }

    Stop(true);

    MaintenanceStatusLabel->set_text("Image file check starting");
    ProgressFraction = 0;

    HasRunSomething = true;
    RunTaskThread = true;
    TaskRunning = true;
    TaskThread = std::thread([this] { _RunTaskThread([this] { _RunFileExistCheck(); }); });

    _UpdateState();
}

void MaintenanceTools::StartDeleteThumbnails()
{
    if (RunTaskThread)
    {
        LOG_ERROR("Already doing an operation can't start a new maintenance operation");
        return;
    }

    Stop(true);

    MaintenanceStatusLabel->set_text("Deleting all thumbnails");
    ProgressFraction = 0;

    HasRunSomething = true;
    RunTaskThread = true;
    TaskRunning = true;
    TaskThread = std::thread([this] { _RunTaskThread([this] { _RunDeleteThumbnails(); }); });

    _UpdateState();
}

void MaintenanceTools::StartDeleteOrphanedResources()
{
    if (RunTaskThread)
    {
        LOG_ERROR("Already doing an operation can't start a new maintenance operation");
        return;
    }

    Stop(true);

    MaintenanceStatusLabel->set_text("Finding resources that weren't correctly deleted...");
    ProgressFraction = 0;

    HasRunSomething = true;
    RunTaskThread = true;
    TaskRunning = true;
    TaskThread = std::thread([this] { _RunTaskThread([this] { _RunDeleteOrphaned(); }); });

    _UpdateState();
}

void MaintenanceTools::StartFixOrphanedResources()
{
    if (RunTaskThread)
    {
        LOG_ERROR("Already doing an operation can't start a new maintenance operation");
        return;
    }

    Stop(true);

    MaintenanceStatusLabel->set_text("Finding orphaned resources to restore...");
    ProgressFraction = 0;

    HasRunSomething = true;
    RunTaskThread = true;
    TaskRunning = true;
    TaskThread = std::thread([this] { _RunTaskThread([this] { _RunFixOrphaned(); }); });

    _UpdateState();
}

// ------------------------------------ //
void MaintenanceTools::_InsertTextResult(const std::string& text)
{
    if (_LimitResults())
        return;

    MaintenanceResultWidgets.push_back(std::make_shared<Gtk::Label>(text));
    auto& widget = *MaintenanceResultWidgets.back();
    MaintenanceResults->add(widget);
    widget.show();
}

void MaintenanceTools::_InsertBrokenImageResult(DBID image, const std::string& brokenPath, const std::string& autoFix)
{
    if (_LimitResults())
        return;

    MaintenanceResultWidgets.push_back(std::make_shared<InvalidImagePathResultWidget>(image, brokenPath, autoFix));
    auto& widget = *MaintenanceResultWidgets.back();
    MaintenanceResults->add(widget);
    widget.show();
}

bool MaintenanceTools::_LimitResults()
{
    if (MaintenanceResultWidgets.size() > MAX_MAINTENANCE_RESULTS)
        return true;

    if (MaintenanceResultWidgets.size() == MAX_MAINTENANCE_RESULTS)
    {
        MaintenanceResultWidgets.push_back(
            std::make_unique<Gtk::Label>("Too many results. Not showing further results."));
        auto& widget = *MaintenanceResultWidgets.back();
        MaintenanceResults->add(widget);
        widget.show();
        return true;
    }

    return false;
}

void MaintenanceTools::_ClearResultsPressed()
{
    for (auto& widget : MaintenanceResultWidgets)
    {
        if (widget)
            MaintenanceResults->remove(*widget);
    }

    MaintenanceResultWidgets.clear();
}

// ------------------------------------ //
void MaintenanceTools::_CancelPressed()
{
    Stop(false);
}

// ------------------------------------ //
void MaintenanceTools::DeleteImportedStagingFolderItemsClicked()
{
    DualView::Get().OpenAlreadyImportedDeleteWindow(DualView::Get().GetSettings().GetStagingFolder());
}

// ------------------------------------ //
void MaintenanceTools::_UpdateState()
{
    MaintenanceSpinner->property_active() = RunTaskThread;
    MaintenanceProgressBar->set_fraction(ProgressFraction);

    if (RunTaskThread)
    {
        MaintenanceCancelButton->set_sensitive(true);
        CheckAllExist->set_sensitive(false);
        DeleteAllThumbnails->set_sensitive(false);
        FixOrphanedResources->set_sensitive(false);
        FixOrphanedImages->set_sensitive(false);
        DeleteImportedStagingFolderItems->set_sensitive(false);
    }
    else
    {
        MaintenanceCancelButton->set_sensitive(false);
        CheckAllExist->set_sensitive(true);
        DeleteAllThumbnails->set_sensitive(true);
        FixOrphanedResources->set_sensitive(true);
        FixOrphanedImages->set_sensitive(true);
        DeleteImportedStagingFolderItems->set_sensitive(true);

        if (HasRunSomething)
        {
            MaintenanceStatusLabel->set_text("Operation finished");
        }
    }
}

void MaintenanceTools::_OnTaskFinished()
{
    LOG_INFO("Maintenance task finished");

    auto alive = GetAliveMarker();

    DualView::Get().RunOnMainThread(
        [=]
        {
            INVOKE_CHECK_ALIVE_MARKER(alive);

            ProgressFraction = 1;
            RunTaskThread = false;
            _UpdateState();
        });
}

// ------------------------------------ //
void MaintenanceTools::_RunTaskThread(const std::function<void()>& operation)
{
    operation();

    if (!RunTaskThread)
    {
        // Canceled
        auto alive = GetAliveMarker();

        DualView::Get().InvokeFunction(
            [=]
            {
                INVOKE_CHECK_ALIVE_MARKER(alive);

                _InsertTextResult("Operation canceled");
            });
    }

    _OnTaskFinished();
}

// ------------------------------------ //
void MaintenanceTools::_RunFileExistCheck()
{
    LOG_INFO("Started check that all db images exist");

    auto alive = GetAliveMarker();

    auto& database = DualView::Get().GetDatabase();

    int64_t total;

    {
        GUARD_LOCK_OTHER(database);
        total = static_cast<int64_t>(database.SelectImageCount(guard));
    }

    LOG_INFO("Total number of images: " + std::to_string(total));

    int64_t processed = 0;

    std::vector<ImagePath> imageBatch;

    while (RunTaskThread)
    {
        if (imageBatch.empty())
        {
            // Need to get a new batch
            try
            {
                GUARD_LOCK_OTHER(database);
                database.SelectImagePaths(guard, imageBatch, processed);
            }
            catch (const Leviathan::Exception& e)
            {
                LOG_ERROR("Failed to get next batch of DB images:");
                e.PrintToLog();

                DualView::Get().InvokeFunction(
                    [=]
                    {
                        INVOKE_CHECK_ALIVE_MARKER(alive);

                        _InsertTextResult("Operation canceled");
                    });

                RunTaskThread = false;
                break;
            }

            // If we get an empty batch that means we have processed all the images
            if (imageBatch.empty())
            {
                LOG_INFO("All db images have been processed in batches");
                break;
            }
        }

        // Process the last image of a batch first as we can then use pop back for more
        // efficiency

        const auto& current = imageBatch.back();

        const auto& path = current.GetPath();

        if (!std::filesystem::exists(path) || std::filesystem::file_size(path) < 1)
        {
            LOG_ERROR("Image path that should exist doesn't (or size is 0): " + path);

            std::string autoFix;

            // Detect if auto-fix is possible
            if (path.find("/ ") != std::string::npos)
            {
                auto potentialFix = Leviathan::StringOperations::Replace<std::string>(path, "/ ", "/");

                if (std::filesystem::exists(potentialFix))
                {
                    autoFix = std::move(potentialFix);
                }
            }
            else if (path.find("...") != std::string::npos)
            {
                auto potentialFix = Leviathan::StringOperations::Replace<std::string>(path, "...", "");

                if (std::filesystem::exists(potentialFix))
                {
                    autoFix = std::move(potentialFix);
                }
            }

            if (!autoFix.empty())
            {
                LOG_INFO("Found auto fix for the above path: " + autoFix);
            }

            DualView::Get().InvokeFunction(
                [=]
                {
                    INVOKE_CHECK_ALIVE_MARKER(alive);

                    _InsertBrokenImageResult(current.GetID(), current.GetPath(), autoFix);
                });
        }

        imageBatch.pop_back();
        ++processed;

        if (processed % IMAGE_CHECK_REPORT_PROGRESS_EVERY_N == 0)
        {
            if (processed > total)
            {
                // More images were probably added since we started
                total = processed;
            }

            DualView::Get().InvokeFunction(
                [=]
                {
                    INVOKE_CHECK_ALIVE_MARKER(alive);

                    ProgressFraction =
                        static_cast<float>((static_cast<double>(processed) / static_cast<double>(total)));
                    MaintenanceStatusLabel->set_text("Checking images");
                    _UpdateState();
                });
        }
    }
}

// ------------------------------------ //
void MaintenanceTools::_RunDeleteThumbnails()
{
    namespace bf = boost::filesystem;

    auto alive = GetAliveMarker();

    const auto thumbnailFolder = DV::DualView::Get().GetThumbnailFolder();

    if (!bf::is_directory(thumbnailFolder))
    {
        // A single file //
        LOG_INFO("Thumbnails folder doesn't exist")

        DualView::Get().InvokeFunction(
            [=]
            {
                INVOKE_CHECK_ALIVE_MARKER(alive);
                _InsertTextResult("Thumbnail folder doesn't exist");
            });

        return;
    }

    LOG_INFO("Started deleting all thumbnails");

    int64_t processed = 0;

    // We can't use a directory iterator as that goes invalid when deleting stuff, so we just delete everything in one
    // go and recreate the folder

    boost::system::error_code error;
    bf::remove_all(thumbnailFolder, error);

    if (error.failed())
    {
        LOG_WARNING("Failed to delete thumbnail folder content: " + error.to_string());
        _InsertTextResult("Error deleting thumbnails: " + error.to_string());
    }

    bf::create_directories(thumbnailFolder, error);

    if (error.failed())
    {
        LOG_WARNING("Failed to recreate thumbnail folder after deleting: " + error.to_string());
        _InsertTextResult("Creating a blank thumbnail folder failed: " + error.to_string());
    }

    DualView::Get().InvokeFunction(
        [=]
        {
            INVOKE_CHECK_ALIVE_MARKER(alive);

            ProgressFraction = 1;
            MaintenanceStatusLabel->set_text("Deleted cached thumbnails");
            _UpdateState();
        });

    LOG_INFO("Thumbnail deleting finished");
}

// ------------------------------------ //
void MaintenanceTools::_RunDeleteOrphaned()
{
    LOG_INFO("Starting check for badly deleted resources");

    auto alive = GetAliveMarker();

    auto& database = DualView::Get().GetDatabase();

    // First detect things to delete
    std::vector<DBID> collectionsToPurge;
    std::vector<DBID> imagesToPurge;
    std::vector<DBID> netGalleriesToPurge;
    std::vector<DBID> foldersToPurge;

    LOG_INFO("Detecting collections to purge");

    {
        GUARD_LOCK_OTHER(database);
        database.SelectIncorrectlyDeletedCollections(guard, collectionsToPurge);
    }

    LOG_INFO("Detecting images to purge");

    if (!RunTaskThread)
        return;

    DualView::Get().InvokeFunction(
        [=]
        {
            INVOKE_CHECK_ALIVE_MARKER(alive);
            MaintenanceStatusLabel->set_text("Detecting badly deleted images...");
        });

    {
        GUARD_LOCK_OTHER(database);
        database.SelectIncorrectlyDeletedImages(guard, imagesToPurge);
    }

    if (!RunTaskThread)
        return;

    LOG_INFO("Detecting net galleries to purge");

    {
        GUARD_LOCK_OTHER(database);
        database.SelectIncorrectlyDeletedNetGalleries(guard, netGalleriesToPurge);
    }

    LOG_INFO("Detecting folders to purge");

    {
        GUARD_LOCK_OTHER(database);
        database.SelectIncorrectlyDeletedFolders(guard, foldersToPurge);
    }

    if (!RunTaskThread)
        return;

    // Then start fixing the detected things
    const auto total = static_cast<int64_t>(
        collectionsToPurge.size() + imagesToPurge.size() + netGalleriesToPurge.size() + foldersToPurge.size());

    if (total < 1)
    {
        LOG_INFO("Nothing to fix badly deleted");

        DualView::Get().InvokeFunction(
            [=]
            {
                INVOKE_CHECK_ALIVE_MARKER(alive);

                _InsertTextResult("No deleted resources need fixing");
            });

        return;
    }

    LOG_INFO("Starting fixing " + std::to_string(total) + " badly deleted resources");
    int64_t processed = 0;

    while (RunTaskThread)
    {
        if (!imagesToPurge.empty())
        {
            const auto resourceId = imagesToPurge.back();
            imagesToPurge.pop_back();

            LOG_INFO("Purging image " + std::to_string(resourceId));

            try
            {
                auto image = database.SelectImageByIDAG(resourceId);

                if (!image || !image->IsDeleted())
                    throw Leviathan::Exception("Target image to delete not found or not deleted");

                image->ForceUnDeleteToFixMissingAction();

                if (!database.DeleteImage(*image))
                    throw Leviathan::Exception("Delete action did not get created for an image");
            }
            catch (const Leviathan::Exception& e)
            {
                LOG_ERROR("Failed to delete orphaned resource:");
                e.PrintToLog();

                DualView::Get().InvokeFunction(
                    [=]
                    {
                        INVOKE_CHECK_ALIVE_MARKER(alive);
                        _InsertTextResult("Orphaned resource deletion failed");
                    });

                RunTaskThread = false;
                break;
            }
        }
        else if (!collectionsToPurge.empty())
        {
            const auto resourceId = collectionsToPurge.back();
            collectionsToPurge.pop_back();

            LOG_INFO("Purging collection " + std::to_string(resourceId));

            try
            {
                auto collection = database.SelectCollectionByIDAG(resourceId);

                if (!collection || !collection->IsDeleted())
                    throw Leviathan::Exception("Target collection to delete not found or not deleted");

                collection->ForceUnDeleteToFixMissingAction();

                if (!database.DeleteCollection(*collection))
                    throw Leviathan::Exception("Delete action did not get created for a collection");
            }
            catch (const Leviathan::Exception& e)
            {
                LOG_ERROR("Failed to delete orphaned resource:");
                e.PrintToLog();

                DualView::Get().InvokeFunction(
                    [=]
                    {
                        INVOKE_CHECK_ALIVE_MARKER(alive);
                        _InsertTextResult("Orphaned resource deletion failed");
                    });

                RunTaskThread = false;
                break;
            }
        }
        else if (!netGalleriesToPurge.empty())
        {
            const auto resourceId = netGalleriesToPurge.back();
            netGalleriesToPurge.pop_back();

            LOG_INFO("Purging NetGallery " + std::to_string(resourceId));

            try
            {
                auto netGallery = database.SelectNetGalleryByIDAG(resourceId);

                if (!netGallery || !netGallery->IsDeleted())
                    throw Leviathan::Exception("Target NetGallery to delete not found or not deleted");

                netGallery->ForceUnDeleteToFixMissingAction();

                if (!database.DeleteNetGallery(*netGallery))
                    throw Leviathan::Exception("Delete action did not get created for a NetGallery");
            }
            catch (const Leviathan::Exception& e)
            {
                LOG_ERROR("Failed to delete orphaned resource:");
                e.PrintToLog();

                DualView::Get().InvokeFunction(
                    [=]
                    {
                        INVOKE_CHECK_ALIVE_MARKER(alive);
                        _InsertTextResult("Orphaned resource deletion failed");
                    });

                RunTaskThread = false;
                break;
            }
        }
        else if (!foldersToPurge.empty())
        {
            const auto resourceId = foldersToPurge.back();
            foldersToPurge.pop_back();

            LOG_INFO("Purging folder " + std::to_string(resourceId));

            try
            {
                auto folder = database.SelectFolderByIDAG(resourceId);

                if (!folder || !folder->IsDeleted())
                    throw Leviathan::Exception("Target folder to delete not found or not deleted");

                folder->ForceUnDeleteToFixMissingAction();

                if (!database.DeleteFolder(*folder))
                    throw Leviathan::Exception("Delete action did not get created for a folder");
            }
            catch (const Leviathan::Exception& e)
            {
                LOG_ERROR("Failed to delete orphaned resource:");
                e.PrintToLog();

                DualView::Get().InvokeFunction(
                    [=]
                    {
                        INVOKE_CHECK_ALIVE_MARKER(alive);
                        _InsertTextResult("Orphaned resource deletion failed");
                    });

                RunTaskThread = false;
                break;
            }
        }
        else
        {
            // All done
            DualView::Get().InvokeFunction(
                [=]
                {
                    INVOKE_CHECK_ALIVE_MARKER(alive);

                    ProgressFraction = 1;
                    MaintenanceStatusLabel->set_text("Fixed badly deleted resources");
                    _InsertTextResult("Deleted " + std::to_string(processed) + " badly deleted resources");
                    _UpdateState();
                });

            break;
        }

        ++processed;

        DualView::Get().InvokeFunction(
            [=]
            {
                INVOKE_CHECK_ALIVE_MARKER(alive);

                ProgressFraction = static_cast<float>((static_cast<double>(processed) / static_cast<double>(total)));
                MaintenanceStatusLabel->set_text("Fixing badly deleted resources");
                _UpdateState();
            });
    }
}

void MaintenanceTools::_RunFixOrphaned()
{
    LOG_INFO("Starting check for orphaned resources");

    auto alive = GetAliveMarker();

    auto& database = DualView::Get().GetDatabase();

    // First detect things to delete
    std::vector<DBID> imagesToAddToUncategorized;

    // TODO: find orphaned folders and collections that are nowhere and put them in root

    LOG_INFO("Detecting images to put in uncategorized");

    if (!RunTaskThread)
        return;

    DualView::Get().InvokeFunction(
        [=]
        {
            INVOKE_CHECK_ALIVE_MARKER(alive);
            MaintenanceStatusLabel->set_text("Detecting orphaned images to put in uncategorized...");
        });

    {
        GUARD_LOCK_OTHER(database);
        database.SelectOrphanedImages(guard, imagesToAddToUncategorized);
    }

    if (!RunTaskThread)
        return;

    // Then start fixing the detected things
    const auto total = static_cast<int64_t>(imagesToAddToUncategorized.size());

    if (total < 1)
    {
        LOG_INFO("Nothing to fix orphaned");

        DualView::Get().InvokeFunction(
            [=]
            {
                INVOKE_CHECK_ALIVE_MARKER(alive);

                _InsertTextResult("No orphaned resources need fixing");
            });

        return;
    }

    LOG_INFO("Starting fixing " + std::to_string(total) + " orphans");
    int64_t processed = 0;

    while (RunTaskThread)
    {
        if (!imagesToAddToUncategorized.empty())
        {
            const auto addToUncategorized = imagesToAddToUncategorized.back();
            imagesToAddToUncategorized.pop_back();

            LOG_INFO("Adding image " + std::to_string(addToUncategorized) + " to uncategorized");

            try
            {
                // This is cached in the DualView object so this is fine to check each loop
                const auto uncategorized = DV::DualView::Get().GetUncategorized();

                if (!uncategorized)
                    throw Leviathan::Exception("Uncategorized collection not found");

                GUARD_LOCK_OTHER(database);

                // If we really cared about performance we would cache the order value between loops
                const auto order = database.SelectCollectionLargestShowOrder(guard, *uncategorized) + 1;
                database.InsertImageToCollection(guard, uncategorized->GetID(), addToUncategorized, order);
            }
            catch (const Leviathan::Exception& e)
            {
                LOG_ERROR("Failed to restore orphaned image to uncategorized:");
                e.PrintToLog();

                DualView::Get().InvokeFunction(
                    [=]
                    {
                        INVOKE_CHECK_ALIVE_MARKER(alive);
                        _InsertTextResult("Failed to restore an image to Uncategorized");
                    });

                RunTaskThread = false;
                break;
            }

            DualView::Get().InvokeFunction(
                [=]
                {
                    INVOKE_CHECK_ALIVE_MARKER(alive);
                    _InsertTextResult("Restored image " + std::to_string(addToUncategorized) + " to Uncategorized");
                });
        }
        else
        {
            // All done
            DualView::Get().InvokeFunction(
                [=]
                {
                    INVOKE_CHECK_ALIVE_MARKER(alive);

                    ProgressFraction = 1;
                    MaintenanceStatusLabel->set_text("Fixed orphaned resources");
                    _UpdateState();
                });

            break;
        }

        ++processed;

        DualView::Get().InvokeFunction(
            [=]
            {
                INVOKE_CHECK_ALIVE_MARKER(alive);

                ProgressFraction = static_cast<float>((static_cast<double>(processed) / static_cast<double>(total)));
                MaintenanceStatusLabel->set_text("Fixing orphaned resources");
                _UpdateState();
            });
    }
}
