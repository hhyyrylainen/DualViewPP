// ------------------------------------ //
#include "ActionEditors.h"

#include "resources/DatabaseAction.h"

#include "Database.h"
#include "DualView.h"

using namespace DV;
// ------------------------------------ //
template<class ActionT>
void EditActionHelper(
    const std::shared_ptr<ActionT>& action, std::function<void()> applychanges)
{
    if(action->IsDeleted())
        throw Leviathan::InvalidState("action must not be deleted");

    // Before applying the changes the action needs to not be performed
    if(action->IsPerformed())
        if(!action->Undo())
            throw Leviathan::InvalidState("undoing the action before applying changes failed");

    applychanges();

    if(!action->Redo())
        throw Leviathan::InvalidState("failed to redo the changed action");
}


ActionEditor::ActionEditor() : Apply("Apply"), MainContainer(Gtk::ORIENTATION_VERTICAL)
{
    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));
    set_default_size(500, 300);

    Menu.set_image_from_icon_name("open-menu-symbolic");
    Menu.set_popover(MenuPopover);

    HeaderBar.property_title() = "Modify action";
    HeaderBar.property_subtitle() = "Loading data...";
    HeaderBar.property_show_close_button() = true;
    HeaderBar.pack_end(Menu);
    set_titlebar(HeaderBar);

    //
    // Content area
    //
    MainContainer.property_vexpand() = true;
    MainContainer.property_hexpand() = true;

    Apply.set_image_from_icon_name("emblem-ok-symbolic");
    Apply.set_always_show_image(true);
    Apply.property_halign() = Gtk::ALIGN_END;
    Apply.signal_clicked().connect(sigc::mem_fun(*this, &ActionEditor::_OnApplyPressed));
    MainContainer.pack_end(Apply, false, false);

    QueryingDatabase.property_active() = true;
    MainArea.add_overlay(QueryingDatabase);

    MainArea.add(MainContainer);

    add(MainArea);
}

ActionEditor::~ActionEditor() {}

void ActionEditor::_OnClose() {}
// ------------------------------------ //
void ActionEditor::_OnDescriptionRetrieved(const std::string& description)
{
    HeaderBar.property_subtitle() = description;
}

void ActionEditor::_SetLoadingStatus(bool loading)
{
    QueryingDatabase.property_visible() = loading;
}
// ------------------------------------ //
void ActionEditor::_OnApplyPressed()
{
    if(!ChangesMade) {
        LOG_INFO("ActionEditor: no changes made before apply");
        close();
        return;
    }

    try {
        _PerformApply();

        // The action succeeded so we can close this window now
        close();

    } catch(const Leviathan::Exception& e) {

        Gtk::Window* parent = dynamic_cast<Gtk::Window*>(this->get_toplevel());

        if(!parent)
            return;

        auto dialog = Gtk::MessageDialog(*parent, "Applying the modified action failed", false,
            Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE, true);

        dialog.set_secondary_text("Error: " + std::string(e.what()));
        dialog.run();
    }
}

// ------------------------------------ //
// MergeActionEditor
MergeActionEditor::MergeActionEditor(const std::shared_ptr<ImageMergeAction>& action) :
    Action(action),
    TargetImageViewer(nullptr, SuperViewer::ENABLED_EVENTS::ALL_BUT_MOVE, false),
    RemoveSelected("_Remove Selected", true)
{
    if(!Action)
        throw Leviathan::InvalidArgument("action must not be null");

    if(Action->IsDeleted())
        throw Leviathan::InvalidState("action must not be deleted");

    // This type specific widgets
    MainContainer.pack_start(TargetLabel, false, true);
    TargetImageViewer.property_height_request() = 280;
    MainContainer.pack_start(TargetImageViewer, true, true);

    MergedImageContainer.property_height_request() = 180;
    MergedImageContainer.property_width_request() = 520;
    MainContainer.pack_start(MergedImageContainer, true, true);

    RemoveSelected.property_sensitive() = false;
    RemoveSelected.signal_clicked().connect(
        sigc::mem_fun(*this, &MergeActionEditor::_RemoveSelectedPressed));
    MainContainer.pack_start(RemoveSelected, false, false);

    show_all_children();
    RefreshData();
}

MergeActionEditor::~MergeActionEditor() {}
// ------------------------------------ //
void MergeActionEditor::RefreshData()
{
    auto isalive = GetAliveMarker();

    DualView::Get().QueueDBThreadFunction([action = this->Action, this, isalive]() {
        auto description = action->GenerateDescription();

        auto previewItems = action->LoadPreviewItems();

        DualView::Get().InvokeFunction(
            [this, isalive, description, previewItems = std::move(previewItems)]() {
                INVOKE_CHECK_ALIVE_MARKER(isalive);

                _OnDescriptionRetrieved(description);

                std::vector<std::shared_ptr<Image>> casted;
                casted.reserve(previewItems.size());

                for(const auto& item : previewItems) {
                    auto castedItem = std::dynamic_pointer_cast<Image>(item);

                    if(castedItem)
                        casted.push_back(castedItem);
                }

                _OnDataRetrieved(casted);
            });
    });
}

void MergeActionEditor::_OnDataRetrieved(const std::vector<std::shared_ptr<Image>>& items)
{
    _SetLoadingStatus(false);

    if(items.empty())
        return;

    TargetImage = items.front();

    TargetLabel.property_label() = "Target image: " + TargetImage->GetName();
    TargetImageViewer.SetImage(TargetImage);

    MergedImages = std::vector<std::shared_ptr<Image>>(items.begin() + 1, items.end());

    RemoveSelected.property_sensitive() = false;

    _UpdateShownItems();
}

void MergeActionEditor::_UpdateShownItems()
{
    MergedImageContainer.SetShownItems(MergedImages.begin(), MergedImages.end(),
        std::make_shared<ItemSelectable>([=](ListItem& item) {
            RemoveSelected.property_sensitive() =
                MergedImageContainer.CountSelectedItems() > 0;
        }));

    // Update the button right away to fix it not getting updated after removing last item
    RemoveSelected.property_sensitive() = MergedImageContainer.CountSelectedItems() > 0;
}
// ------------------------------------ //
void MergeActionEditor::_RemoveSelectedPressed()
{
    std::vector<std::shared_ptr<ResourceWithPreview>> selected;
    MergedImageContainer.GetSelectedItems(selected);

    MergedImages.erase(std::remove_if(
        MergedImages.begin(), MergedImages.begin(), [&](const std::shared_ptr<Image>& image) {
            return std::find(selected.begin(), selected.end(), image) != selected.end();
        }));

    ChangesMade = true;
    _UpdateShownItems();
}
// ------------------------------------ //
void MergeActionEditor::_PerformApply()
{
    if(MergedImages.size() < 1)
        throw Leviathan::Exception(
            "the edited action is invalid as there are no longer any merged images");

    EditActionHelper(Action, [&]() {
        std::vector<DBID> ids;
        std::transform(MergedImages.begin(), MergedImages.end(), std::back_inserter(ids),
            [](const std::shared_ptr<Image>& image) { return image->GetID(); });

        Action->UpdateProperties(TargetImage->GetID(), ids);

        // TODO: could maybe load the old state from the DB
        // The old state is not stored so we may as well save it now
        Action->Save();
    });
}
