// ------------------------------------ //
#include "AlreadyImportedImageDeleter.h"

#include <Exceptions.h>
#include <FileSystem.h>
#include <filesystem>

#include "Common.h"
#include "DualView.h"

#include "resources/Image.h"

#include "Database.h"

constexpr auto REPORT_PROGRESS_EVERY_N_ITEMS = 10;

using namespace DV;

// ------------------------------------ //
AlreadyImportedImageDeleter::AlreadyImportedImageDeleter(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &AlreadyImportedImageDeleter::_OnClose));

    signal_unmap().connect(sigc::mem_fun(*this, &AlreadyImportedImageDeleter::_OnHidden));
    signal_map().connect(sigc::mem_fun(*this, &AlreadyImportedImageDeleter::_OnShown));

    // Get and apply primary menu options
    BUILDER_GET_PRIMARY_MENU_NAMED("MenuButtonAlreadyImported", Menu, MenuPopover);

    BUILDER_GET_WIDGET(AlreadyImportedCheckPathChooser);
    AlreadyImportedCheckPathChooser->signal_selection_changed().connect(
        sigc::mem_fun(*this, &AlreadyImportedImageDeleter::_OnSelectedPathChanged));

    BUILDER_GET_WIDGET(AlreadyImportedStartStopButton);
    AlreadyImportedStartStopButton->signal_clicked().connect(
        sigc::mem_fun(*this, &AlreadyImportedImageDeleter::OnStartStopPressed));

    BUILDER_GET_WIDGET(AlreadyImportedProcessingSpinner);
    BUILDER_GET_WIDGET(AlreadyImportedStatusLabel);
    BUILDER_GET_WIDGET(AlreadyImportedFilesCheckedLabel);
}

AlreadyImportedImageDeleter::~AlreadyImportedImageDeleter()
{
    Stop(false);
}

// ------------------------------------ //
bool AlreadyImportedImageDeleter::_OnClose(GdkEventAny* event)
{
    // Just hide it //
    hide();
    _OnHidden();
    return true;
}

void AlreadyImportedImageDeleter::_OnHidden()
{
    Stop(true);
    AlreadyImportedCheckPathChooser->set_sensitive(true);
}

void AlreadyImportedImageDeleter::_OnShown()
{
    _UpdateButtonState();
}

// ------------------------------------ //
void AlreadyImportedImageDeleter::OnStartStopPressed()
{
    if (StopProcessing)
    {
        Stop(true);
        Start();
    }
    else
    {
        Stop(false);
    }
}

// ------------------------------------ //
void AlreadyImportedImageDeleter::Stop(bool wait)
{
    if (StopProcessing)
    {
        if (wait && !ThreadJoined)
        {
            TaskThread.join();
            ThreadJoined = true;
        }

        return;
    }

    StopProcessing = true;

    if (wait)
    {
        TaskThread.join();
        ThreadJoined = true;
    }

    AlreadyImportedCheckPathChooser->set_sensitive(true);

    _UpdateButtonState();
}

void AlreadyImportedImageDeleter::Start()
{
    if (!StopProcessing)
        return;

    // Make sure thread is joined
    Stop(true);

    TargetFolderToProcess = AlreadyImportedCheckPathChooser->get_filename();

    StopProcessing = false;
    ThreadJoined = false;
    TaskThread = std::thread([this] { _RunTaskThread(); });

    AlreadyImportedCheckPathChooser->set_sensitive(false);

    _UpdateButtonState();
}

// ------------------------------------ //
void AlreadyImportedImageDeleter::SetSelectedFolder(const std::string& path)
{
    if (!StopProcessing)
    {
        LOG_ERROR("AlreadyImportedImageDeleter: Can't override path while currently running");
        return;
    }

    if (!std::filesystem::exists(path))
    {
        LOG_ERROR("AlreadyImportedImageDeleter: not overriding path with a not existing one");
        return;
    }

    AlreadyImportedCheckPathChooser->set_filename(path);
}

// ------------------------------------ //
void AlreadyImportedImageDeleter::_UpdateButtonState()
{
    if (StopProcessing)
    {
        AlreadyImportedStartStopButton->get_style_context()->add_class("suggested-action");
        AlreadyImportedStartStopButton->get_style_context()->remove_class("destructive-action");
        AlreadyImportedStartStopButton->set_label("Start");
        AlreadyImportedProcessingSpinner->property_active() = false;
        AlreadyImportedStatusLabel->set_text("Stopped");
    }
    else
    {
        AlreadyImportedStartStopButton->get_style_context()->remove_class("suggested-action");
        AlreadyImportedStartStopButton->get_style_context()->add_class("destructive-action");
        AlreadyImportedStartStopButton->set_label("Stop");
        AlreadyImportedProcessingSpinner->property_active() = true;
        AlreadyImportedStatusLabel->set_text("Starting");
    }
}

void AlreadyImportedImageDeleter::_OnSelectedPathChanged()
{
    AlreadyImportedStartStopButton->set_sensitive(!AlreadyImportedCheckPathChooser->get_filename().empty());
}

// ------------------------------------ //
void AlreadyImportedImageDeleter::_RunTaskThread()
{
    int items = 0;

    std::filesystem::recursive_directory_iterator iterator(TargetFolderToProcess);
    std::filesystem::recursive_directory_iterator end;

    auto alive = GetAliveMarker();
    std::string contents;

    auto& database = DualView::Get().GetDatabase();

    while (!StopProcessing)
    {
        if (iterator == end)
        {
            // Ended
            StopProcessing = true;

            DualView::Get().InvokeFunction(
                [=]()
                {
                    INVOKE_CHECK_ALIVE_MARKER(alive);

                    _UpdateButtonState();
                    AlreadyImportedStatusLabel->set_text("All files processed");
                });

            break;
        }

        std::string currentPath;

        try
        {
            const auto status = iterator->status();

            // Skip folders
            if (!std::filesystem::is_regular_file(status))
            {
                ++iterator;
                continue;
            }

            const auto currentSize = std::filesystem::file_size(iterator->path());

            // and empty files
            if (currentSize < 1)
            {
                ++iterator;
                continue;
            }

            // TODO: add a button to select between processing all files and only ones with
            // content extensions

            currentPath = iterator->path().string();

            if (!Leviathan::FileSystem::ReadFileEntirely(currentPath, contents))
            {
                throw Exception("Failed to read contents of: " + currentPath);
            }

            ++iterator;

            const auto hash = DualView::CalculateBase64EncodedHash(contents);

            std::shared_ptr<Image> existing;
            {
                GUARD_LOCK_OTHER(database);
                existing = database.SelectImageByHash(guard, hash);
            }

            if (existing)
            {
                LOG_INFO("Found already existing image (" + hash + ") at path: " + currentPath +
                    " path in collection: " + existing->GetResourcePath());

                if (std::filesystem::equivalent(currentPath, existing->GetResourcePath()))
                {
                    LOG_WARNING("Just checked a file path that was within the collection, "
                                "ignoring as this is dangerous");
                    continue;
                }

                bool deleteChecked = true;

                if (!std::filesystem::exists(existing->GetResourcePath()) ||
                    std::filesystem::file_size(existing->GetResourcePath()) != currentSize)
                {
                    LOG_ERROR("Detected an existing image that doesn't exist (or is the wrong "
                              "size) at: " +
                        existing->GetResourcePath());
                    LOG_INFO("Trying to fix the non-existing file by moving currently checked "
                             "image");

                    if (!DualView::MoveFile(currentPath, existing->GetResourcePath()))
                    {
                        throw Exception("Failed to move file to repair collection file");
                    }

                    deleteChecked = false;
                    ++TotalItemsCopiedToRepairCollection;
                }

                if (deleteChecked)
                {
                    LOG_INFO("File at path already exists in collection, deleting: " + currentPath);
                    std::filesystem::remove(currentPath);
                    ++TotalItemsDeleted;
                }
            }
        }
        catch (const std::exception& e)
        {
            const std::string error(e.what());
            LOG_ERROR("Error while processing AlreadyImportedImageDeleter: " + error);
            StopProcessing = true;

            DualView::Get().InvokeFunction(
                [=]()
                {
                    INVOKE_CHECK_ALIVE_MARKER(alive);

                    _UpdateButtonState();
                    AlreadyImportedStatusLabel->set_text("Error processing some file: " + error);
                });

            return;
        }

        // Report progress
        ++TotalItemsProcessed;
        ++items;

        if (items % REPORT_PROGRESS_EVERY_N_ITEMS == 0)
        {
            DualView::Get().InvokeFunction(
                [=]()
                {
                    INVOKE_CHECK_ALIVE_MARKER(alive);

                    AlreadyImportedStatusLabel->set_text("Processed: " + currentPath);
                    AlreadyImportedFilesCheckedLabel->set_text(std::to_string(TotalItemsProcessed) +
                        " items processed " + std::to_string(TotalItemsDeleted) + " existing items deleted " +
                        std::to_string(TotalItemsCopiedToRepairCollection) + " items used for collection repair");
                });
        }
    }
}
