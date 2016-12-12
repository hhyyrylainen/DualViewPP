// ------------------------------------ //
#include "RemoveFromFolders.h"

#include "core/resources/Collection.h"

#include "core/DualView.h"

using namespace DV;
// ------------------------------------ //

RemoveFromFolders::RemoveFromFolders(std::shared_ptr<Collection> collection) :
    TargetCollection(collection),
    
    MainBox(Gtk::ORIENTATION_VERTICAL),
    ApplyButton(Gtk::StockID("gtk-apply"))
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

    auto* editrender = dynamic_cast<Gtk::CellRendererToggle*>(
        FoldersTreeView.get_column_cell_renderer(0));

    LEVIATHAN_ASSERT(editrender, "RemoveFromFolders column type dynamic cast failed");
    editrender->signal_toggled().connect(sigc::mem_fun(this, &RemoveFromFolders::_OnToggled));

    FoldersModel = Gtk::ListStore::create(TreeViewColumns);
    FoldersTreeView.set_model(FoldersModel);
    
    
    show_all_children();

    set_default_size(600, 650);

    ReadFolders();
}

RemoveFromFolders::~RemoveFromFolders(){

    Close();
}
// ------------------------------------ //
void RemoveFromFolders::_OnClose(){

    
}
// ------------------------------------ //
void RemoveFromFolders::_OnApply(){

    std::vector<std::string> pathstoremove;

    // Find folders to delete //
    const auto& rows = FoldersModel->children();
    for(auto iter = rows.begin(); iter != rows.end(); ++iter){

        Gtk::TreeModel::Row row = *iter;

        if(!static_cast<bool>(row[TreeViewColumns.m_keep_folder])){

            pathstoremove.push_back(static_cast<Glib::ustring>(
                    row[TreeViewColumns.m_folder_path]));
        }
    }

    if(pathstoremove.empty()){

        close();
        return;
    }

    LOG_INFO("Removing collection: " + TargetCollection->GetName() + " from:");
    
    for(const auto &path : pathstoremove){

        LOG_WRITE("\t" + path);
    }

    set_sensitive(false);


    DualView::Get().QueueDBThreadFunction([=](){

            
        });
    
    
    close();
}
// ------------------------------------ //
void RemoveFromFolders::_OnToggled(const Glib::ustring& path){
    
    Gtk::TreeModel::Row row = *FoldersModel->get_iter(path);
    
    row[TreeViewColumns.m_keep_folder] = !static_cast<bool>(
        row[TreeViewColumns.m_keep_folder]);
}
 
void RemoveFromFolders::ReadFolders(){

    DualView::IsOnMainThreadAssert();

    auto collection = TargetCollection;
    auto alive = GetAliveMarker();

    DualView::Get().QueueDBThreadFunction([=](){

            auto folders = DualView::Get().GetFoldersCollectionIsIn(collection);

            std::sort(folders.begin(), folders.end());

            DualView::Get().InvokeFunction([=](){

                    INVOKE_CHECK_ALIVE_MARKER(alive);

                    FoldersModel->clear();
                    
                    for(const auto& folder : folders){
        
                        Gtk::TreeModel::Row row = *(FoldersModel->append());
                        row[TreeViewColumns.m_keep_folder] = true;
                        row[TreeViewColumns.m_folder_path] = folder;
                    }
                });
        });
}
