// ------------------------------------ //
#include "RemoveFromFolders.h"

#include "resources/Collection.h"
#include "resources/Folder.h"

#include "DualView.h"

using namespace DV;
// ------------------------------------ //

RemoveFromFolders::RemoveFromFolders() :
    MainBox(Gtk::ORIENTATION_VERTICAL), ApplyButton("_Apply", true)
{
    ApplyButton.set_always_show_image();
    MainBox.pack_end(ApplyButton, false, true);
    add(MainBox);

    ApplyButton.signal_clicked().connect(sigc::mem_fun(this, &RemoveFromFolders::_OnApply));

    FoldersTreeView.append_column_editable("Keep", TreeViewColumns.m_keep_folder);
    FoldersTreeView.append_column("In Folder", TreeViewColumns.m_folder_path);

    FoldersTreeView.get_selection()->set_mode(Gtk::SELECTION_NONE);

    MainBox.pack_start(FoldersTreeView, true, true);

    auto* textcolumn = FoldersTreeView.get_column(1);
    textcolumn->set_expand(true);

    auto* editrender =
        dynamic_cast<Gtk::CellRendererToggle*>(FoldersTreeView.get_column_cell_renderer(0));

    LEVIATHAN_ASSERT(editrender, "RemoveFromFolders column type dynamic cast failed");
    editrender->signal_toggled().connect(sigc::mem_fun(this, &RemoveFromFolders::_OnToggled));

    FoldersModel = Gtk::ListStore::create(TreeViewColumns);
    FoldersTreeView.set_model(FoldersModel);


    show_all_children();

    set_default_size(600, 650);
}

RemoveFromFolders::RemoveFromFolders(std::shared_ptr<Collection> collection) :
    RemoveFromFolders()
{
    TargetCollection = collection;
    _UpdateLabelsForType();
    ReadFolders();
}

RemoveFromFolders::RemoveFromFolders(std::shared_ptr<Folder> folder) : RemoveFromFolders()
{
    TargetFolder = folder;
    _UpdateLabelsForType();
    ReadFolders();
}

RemoveFromFolders::~RemoveFromFolders()
{
    Close();
}
// ------------------------------------ //
void RemoveFromFolders::_OnClose() {}
// ------------------------------------ //
void RemoveFromFolders::_OnApply()
{
    std::vector<std::string> pathstoremove;

    // Find folders to delete //
    const auto& rows = FoldersModel->children();
    for(auto iter = rows.begin(); iter != rows.end(); ++iter) {

        Gtk::TreeModel::Row row = *iter;

        if(!static_cast<bool>(row[TreeViewColumns.m_keep_folder])) {

            pathstoremove.push_back(
                static_cast<Glib::ustring>(row[TreeViewColumns.m_folder_path]));
        }
    }

    if(pathstoremove.empty()) {

        close();
        return;
    }

    if(TargetCollection) {
        LOG_INFO("Removing collection: " + TargetCollection->GetName() + " from:");
    } else if(TargetFolder) {
        LOG_INFO("Removing folder: " + TargetFolder->GetName() + " from:");
    }

    for(const auto& path : pathstoremove) {

        LOG_WRITE("\t" + path);
    }

    set_sensitive(false);

    auto alive = GetAliveMarker();

    if(TargetCollection) {
        auto collection = TargetCollection;

        DualView::Get().QueueDBThreadFunction([=]() {
            for(const auto& path : pathstoremove) {

                auto folder = DualView::Get().GetFolderFromPath(path);

                if(!folder) {

                    LOG_ERROR("RemoveFromFolder: path is invalid: " + path);
                    continue;
                }

                DualView::Get().RemoveCollectionFromFolder(collection, folder);
            }

            DualView::Get().InvokeFunction([=]() {
                INVOKE_CHECK_ALIVE_MARKER(alive);
                close();
            });
        });
    } else if(TargetFolder) {
        auto childFolder = TargetFolder;

        DualView::Get().QueueDBThreadFunction([=]() {
            for(const auto& path : pathstoremove) {

                auto folder = DualView::Get().GetFolderFromPath(path);

                if(!folder) {

                    LOG_ERROR("RemoveFromFolder: path is invalid: " + path);
                    continue;
                }

                if(!folder->RemoveFolder(childFolder)) {
                    LOG_ERROR("RemoveFromFolder: failed to remove a child from parent");
                }
            }

            DualView::Get().InvokeFunction([=]() {
                INVOKE_CHECK_ALIVE_MARKER(alive);
                close();
            });
        });
    } else {
        LOG_FATAL("Unhandled RemoveFromFoldersType");
    }
}
// ------------------------------------ //
void RemoveFromFolders::_OnToggled(const Glib::ustring& path)
{
    Gtk::TreeModel::Row row = *FoldersModel->get_iter(path);

    row[TreeViewColumns.m_keep_folder] =
        !static_cast<bool>(row[TreeViewColumns.m_keep_folder]);
}

void RemoveFromFolders::ReadFolders()
{
    DualView::IsOnMainThreadAssert();
    auto alive = GetAliveMarker();

    if(TargetCollection) {
        auto collection = TargetCollection;

        DualView::Get().QueueDBThreadFunction([=]() {
            auto folders = DualView::Get().GetFoldersCollectionIsIn(collection);

            std::sort(folders.begin(), folders.end());

            DualView::Get().InvokeFunction([=]() {
                INVOKE_CHECK_ALIVE_MARKER(alive);
                _UpdateModel(folders);
            });
        });
    } else if(TargetFolder) {
        auto folder = TargetFolder;

        DualView::Get().QueueDBThreadFunction([=]() {
            auto folders = DualView::Get().GetFoldersFolderIsIn(folder);

            std::sort(folders.begin(), folders.end());

            DualView::Get().InvokeFunction([=]() {
                INVOKE_CHECK_ALIVE_MARKER(alive);
                _UpdateModel(folders);
            });
        });
    } else {
        LOG_FATAL("Unhandled RemoveFromFoldersType");
    }
}

void RemoveFromFolders::_UpdateModel(const std::vector<std::string>& folders)
{
    FoldersModel->clear();

    for(const auto& folder : folders) {

        Gtk::TreeModel::Row row = *(FoldersModel->append());
        row[TreeViewColumns.m_keep_folder] = true;
        row[TreeViewColumns.m_folder_path] = folder;
    }
}
// ------------------------------------ //
void RemoveFromFolders::_UpdateLabelsForType()
{
    if(TargetCollection) {

    } else if(TargetFolder) {

    } else {
        throw Leviathan::InvalidState("No target collection or folder for RemoveFromFolders");
    }
}
