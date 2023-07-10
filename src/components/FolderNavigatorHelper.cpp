// ------------------------------------ //
#include "FolderNavigatorHelper.h"

#include "Common.h"
#include "DualView.h"

using namespace DV;

// ------------------------------------ //
void FolderNavigatorHelper::GoToRoot()
{
    GoToPath(VirtualPath("Root/"));
}

void FolderNavigatorHelper::GoToPath(const VirtualPath& path)
{
    auto alive = GetAliveMarker();

    DualView::Get().QueueDBThreadFunction(
        [=]()
        {
            auto folder = DualView::Get().GetFolderFromPath(path);

            DualView::Get().InvokeFunction(
                [=]()
                {
                    INVOKE_CHECK_ALIVE_MARKER(alive);

                    CurrentFolder = folder;
                    CurrentPath = path;

                    if (!CurrentFolder)
                        GoToRoot();

                    OnFolderChanged();
                });
        });
}

// ------------------------------------ //
std::future<bool> FolderNavigatorHelper::TryGoToPath(const VirtualPath& path)
{
    auto result = std::make_shared<std::promise<bool>>();
    auto alive = GetAliveMarker();

    DualView::Get().QueueDBThreadFunction(
        [=]()
        {
            auto folder = DualView::Get().GetFolderFromPath(path);

            if (!folder)
            {
                result->set_value(false);
                return;
            }

            DualView::Get().InvokeFunction(
                [=]()
                {
                    INVOKE_CHECK_ALIVE_MARKER(alive);

                    CurrentFolder = folder;
                    CurrentPath = path;

                    OnFolderChanged();

                    result->set_value(true);
                });
        });

    return result->get_future();
}

// ------------------------------------ //
void FolderNavigatorHelper::MoveToSubfolder(const std::string& subfoldername)
{
    if (subfoldername.empty())
        return;

    CurrentPath = CurrentPath / VirtualPath(subfoldername);
    GoToPath(CurrentPath);
}

// ------------------------------------ //
void FolderNavigatorHelper::_OnUpFolder()
{
    --CurrentPath;
    GoToPath(CurrentPath);
}

void FolderNavigatorHelper::_OnPathEntered()
{
    if (!NavigatorPathEntry)
        return;

    auto checkready =
        std::make_shared<std::future<bool>>(TryGoToPath(VirtualPath(NavigatorPathEntry->get_text(), false)));

    DualView::Get().QueueConditional(
        [=]() -> bool
        {
            const auto ready = checkready->wait_for(std::chrono::seconds(0));

            if (ready == std::future_status::timeout)
                return false;

            const bool result = checkready->get();

            if (!result)
            {
                // DualView::Get().InvokeFunction();
                LOG_ERROR("FolderNavigator: TODO: error sound");
            }

            return true;
        });
}

void FolderNavigatorHelper::RegisterNavigator(Gtk::Entry& pathentry, Gtk::Button& upfolder)
{
    upfolder.signal_clicked().connect(sigc::mem_fun(*this, &FolderNavigatorHelper::_OnUpFolder));

    pathentry.signal_activate().connect(sigc::mem_fun(*this, &FolderNavigatorHelper::_OnPathEntered));

    NavigatorPathEntry = &pathentry;
}
