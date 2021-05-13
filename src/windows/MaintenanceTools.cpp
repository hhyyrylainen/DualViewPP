// ------------------------------------ //
#include "MaintenanceTools.h"

#include "Database.h"
#include "DualView.h"

#include "resources/Image.h"
#include "resources/ImagePath.h"

#include "Common.h"

#include "Common/StringOperations.h"
#include "Exceptions.h"

#include <filesystem>

constexpr auto IMAGE_CHECK_REPORT_PROGRESS_EVERY_N = 200;
constexpr auto MAX_MAINTENANCE_RESULTS = 10000;

using namespace DV;
// ------------------------------------ //
// InvalidImagePathResultWidget
class InvalidImagePathResultWidget : public Gtk::Box, IsAlive {
public:
    InvalidImagePathResultWidget(
        DBID imageId, const std::string& invalidPath, const std::string& autoFix) :
        Gtk::Box(Gtk::ORIENTATION_VERTICAL),
        TopHalf(Gtk::ORIENTATION_HORIZONTAL), BottomHalf(Gtk::ORIENTATION_HORIZONTAL),
        AutoFixButton("Apply AutoFix"), DeleteButton("Delete"), IgnoreButton("Ignore For Now"),
        ImageID(imageId), Path(invalidPath), AutoFix(autoFix)
    {
        TopHalf.pack_start(Working, false, true);
        InfoLabel.set_selectable(true);
        InfoLabel.set_line_wrap_mode(Pango::WRAP_CHAR);
        InfoLabel.set_max_width_chars(80);

        std::string text =
            "Image " + std::to_string(imageId) + " doesn't exist at path: " + invalidPath;

        if(HasAutoFix()) {
            text += " Can be automatically fixed to: " + AutoFix;
        }

        InfoLabel.set_text(text);
        TopHalf.pack_end(InfoLabel, true, true);
        pack_start(TopHalf, true, true);

        DeleteButton.get_style_context()->add_class("destructive-action");

        AutoFixButton.signal_clicked().connect(
            sigc::mem_fun(*this, &InvalidImagePathResultWidget::_AutoFixPressed));

        if(HasAutoFix())
            BottomHalf.pack_start(AutoFixButton);

        DeleteButton.signal_clicked().connect(
            sigc::mem_fun(*this, &InvalidImagePathResultWidget::_DeletePressed));
        BottomHalf.pack_start(DeleteButton);

        IgnoreButton.signal_clicked().connect(
            sigc::mem_fun(*this, &InvalidImagePathResultWidget::_IgnorePressed));
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
        if(!HasAutoFix())
            return;

        Working.property_active() = true;
        _SetButtonStatuses(false);

        LOG_INFO("Auto fixing image: " + std::to_string(ImageID) + " path to: " + AutoFix);

        auto alive = GetAliveMarker();

        DualView::Get().QueueDBThreadFunction([=, id = ImageID, fix = AutoFix]() {
            auto& database = DualView::Get().GetDatabase();
            GUARD_LOCK_OTHER(database);

            const auto image = database.SelectImageByID(guard, id);

            std::string message;

            if(!image) {
                LOG_ERROR("Could not find image for fixing by id: " + std::to_string(ImageID));
                message = "Failed to find target image";
            } else {

                image->SetResourcePath(fix);
                image->Save(database, guard);

                message = "Successfully saved updated path";
                LOG_INFO(message);
            }

            DualView::Get().InvokeFunction([=]() {
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

        DualView::Get().QueueDBThreadFunction([=, id = ImageID, fix = AutoFix]() {
            auto& database = DualView::Get().GetDatabase();
            GUARD_LOCK_OTHER(database);

            const auto image = database.SelectImageByID(guard, id);

            std::string message;

            if(!image) {
                LOG_ERROR(
                    "Could not find image for deletion by id: " + std::to_string(ImageID));
                message = "Failed to find target image";
            } else if(image->IsDeleted()) {
                message = "Image was already deleted";
            } else {

                database.DeleteImage(*image);

                message = "Image deleted successfully";
                LOG_INFO(message);
            }

            DualView::Get().InvokeFunction([=]() {
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
MaintenanceTools::MaintenanceTools(_GtkWindow* window, Glib::RefPtr<Gtk::Builder> builder) :
    Gtk::Window(window)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &MaintenanceTools::_OnClose));

    signal_unmap().connect(sigc::mem_fun(*this, &MaintenanceTools::_OnHidden));
    signal_map().connect(sigc::mem_fun(*this, &MaintenanceTools::_OnShown));

    // Get and apply primary menu options
    BUILDER_GET_PRIMARY_MENU_NAMED("MenuButtonMaintenance", Menu, MenuPopover);

    BUILDER_GET_WIDGET(MaintenanceCancelButton);
    MaintenanceCancelButton->signal_clicked().connect(
        sigc::mem_fun(*this, &MaintenanceTools::_CancelPressed));

    BUILDER_GET_WIDGET(CheckAllExist);
    CheckAllExist->signal_clicked().connect(
        sigc::mem_fun(*this, &MaintenanceTools::StartImageExistCheck));

    BUILDER_GET_WIDGET(MaintenanceResults);
    BUILDER_GET_WIDGET(MaintenanceClearResults);
    MaintenanceClearResults->signal_clicked().connect(
        sigc::mem_fun(*this, &MaintenanceTools::_ClearResultsPressed));

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

    if(wait && TaskRunning) {
        TaskThread.join();
        TaskRunning = false;
    }

    _UpdateState();
}
// ------------------------------------ //
void MaintenanceTools::StartImageExistCheck()
{
    if(RunTaskThread) {
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
// ------------------------------------ //
void MaintenanceTools::_InsertTextResult(const std::string& text)
{
    if(_LimitResults())
        return;

    MaintenanceResultWidgets.push_back(std::make_shared<Gtk::Label>(text));
    auto& widget = *MaintenanceResultWidgets.back();
    MaintenanceResults->add(widget);
    widget.show();
}

void MaintenanceTools::_InsertBrokenImageResult(
    DBID image, const std::string& brokenPath, const std::string& autoFix)
{
    if(_LimitResults())
        return;

    MaintenanceResultWidgets.push_back(
        std::make_shared<InvalidImagePathResultWidget>(image, brokenPath, autoFix));
    auto& widget = *MaintenanceResultWidgets.back();
    MaintenanceResults->add(widget);
    widget.show();
}

bool MaintenanceTools::_LimitResults()
{
    if(MaintenanceResultWidgets.size() > MAX_MAINTENANCE_RESULTS)
        return true;

    if(MaintenanceResultWidgets.size() == MAX_MAINTENANCE_RESULTS) {
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
    for(auto& widget : MaintenanceResultWidgets) {
        if(widget)
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
void MaintenanceTools::_UpdateState()
{
    MaintenanceSpinner->property_active() = RunTaskThread;
    MaintenanceProgressBar->set_fraction(ProgressFraction);

    if(RunTaskThread) {
        MaintenanceCancelButton->set_sensitive(true);
        CheckAllExist->set_sensitive(false);

    } else {
        MaintenanceCancelButton->set_sensitive(false);
        CheckAllExist->set_sensitive(true);

        if(HasRunSomething) {
            MaintenanceStatusLabel->set_text("Operation finished");
        }
    }
}

void MaintenanceTools::_OnTaskFinished()
{
    LOG_INFO("Maintenance task finished");

    auto alive = GetAliveMarker();

    DualView::Get().RunOnMainThread([=] {
        INVOKE_CHECK_ALIVE_MARKER(alive);

        ProgressFraction = 1;
        RunTaskThread = false;
        _UpdateState();
    });
}
// ------------------------------------ //
void MaintenanceTools::_RunTaskThread(std::function<void()> operation)
{
    operation();

    if(!RunTaskThread) {
        // Canceled
        auto alive = GetAliveMarker();

        DualView::Get().InvokeFunction([=] {
            INVOKE_CHECK_ALIVE_MARKER(alive);

            _InsertTextResult("Operation canceled");
        });
    }

    _OnTaskFinished();
}

void MaintenanceTools::_RunFileExistCheck()
{
    LOG_INFO("Started check that all db images exist");

    auto alive = GetAliveMarker();

    auto& database = DualView::Get().GetDatabase();

    int total;

    {
        GUARD_LOCK_OTHER(database);
        total = database.SelectImageCount(guard);
    }

    LOG_INFO("Total number of images: " + std::to_string(total));

    int processed = 0;

    std::vector<ImagePath> imageBatch;

    while(RunTaskThread) {
        if(imageBatch.empty()) {
            // Need to get a new batch
            try {
                GUARD_LOCK_OTHER(database);
                database.SelectImagePaths(guard, imageBatch, processed);
            } catch(const Leviathan::Exception& e) {
                LOG_ERROR("Failed to get next batch of DB images:");
                e.PrintToLog();

                DualView::Get().InvokeFunction([=] {
                    INVOKE_CHECK_ALIVE_MARKER(alive);

                    _InsertTextResult("Operation canceled");
                });

                RunTaskThread = false;
                break;
            }

            // If we get an empty batch that means we have processed all of the images
            if(imageBatch.empty()) {
                LOG_INFO("All db images have been processed in batches");
                break;
            }
        }

        // Process the last image of a batch first as we can then use pop back for more
        // efficiency

        const auto& current = imageBatch.back();

        const auto& path = current.GetPath();

        if(!std::filesystem::exists(path) || std::filesystem::file_size(path) < 1) {
            LOG_ERROR("Image path that should exist doesn't (or size is 0): " + path);

            std::string autoFix;

            // Detect if auto-fix is possible
            if(path.find("/ ") != path.npos) {

                auto potentialFix =
                    Leviathan::StringOperations::Replace<std::string>(path, "/ ", "/");

                if(std::filesystem::exists(potentialFix)) {
                    autoFix = std::move(potentialFix);
                }
            } else if(path.find("...") != path.npos) {
                auto potentialFix =
                    Leviathan::StringOperations::Replace<std::string>(path, "...", "");

                if(std::filesystem::exists(potentialFix)) {
                    autoFix = std::move(potentialFix);
                }
            }

            if(!autoFix.empty()) {
                LOG_INFO("Found auto fix for the above path: " + autoFix);
            }

            DualView::Get().InvokeFunction([=] {
                INVOKE_CHECK_ALIVE_MARKER(alive);

                _InsertBrokenImageResult(current.GetID(), current.GetPath(), autoFix);
            });
        }

        imageBatch.pop_back();
        ++processed;

        if(processed % IMAGE_CHECK_REPORT_PROGRESS_EVERY_N == 0) {

            if(processed > total) {
                // More images were probably added since we started
                total = processed;
            }

            DualView::Get().InvokeFunction([=] {
                INVOKE_CHECK_ALIVE_MARKER(alive);

                ProgressFraction = static_cast<float>(processed) / total;
                MaintenanceStatusLabel->set_text("Checking images");
                _UpdateState();
            });
        }
    }
}
