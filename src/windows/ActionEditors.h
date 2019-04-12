#pragma once

#include "BaseWindow.h"
#include "IsAlive.h"

#include "components/PrimaryMenu.h"
#include "components/SuperContainer.h"
#include "components/SuperViewer.h"


#include "Common/BaseNotifier.h"

#include <gtkmm.h>

#include <atomic>

namespace DV {

class ImageMergeAction;

//! \brief Base for all windows that allow editing actions
class ActionEditor : public BaseWindow,
                     public Gtk::Window,
                     public IsAlive,
                     public BaseNotifierAll {
public:
    ActionEditor(BaseNotifiableAll* notify);
    ~ActionEditor();

protected:
    void _OnClose() override;

    void _SetLoadingStatus(bool loading);

    void _OnApplyPressed();

    void _OnDescriptionRetrieved(const std::string& description);

    virtual void _PerformApply() = 0;

private:
    // Titlebar widgets
    Gtk::HeaderBar HeaderBar;
    Gtk::MenuButton Menu;

    // Primary menu
    PrimaryMenu MenuPopover;

    // Main content area
    Gtk::Overlay MainArea;
    Gtk::Spinner QueryingDatabase;

    Gtk::Button Apply;

protected:
    Gtk::Box MainContainer;

    //! Used to skip applying changes when not necessary
    bool ChangesMade = false;

    // Actual widgets are in the child classes
};

//! \brief Editor for ImageMergeAction
class MergeActionEditor final : public ActionEditor {
public:
    MergeActionEditor(
        const std::shared_ptr<ImageMergeAction>& action, BaseNotifiableAll* notify);
    ~MergeActionEditor();

    void RefreshData();

protected:
    void _PerformApply() override;

    void _OnDataRetrieved(const std::vector<std::shared_ptr<Image>>& items);

    void _RemoveSelectedPressed();
    void _UpdateShownItems();

private:
    const std::shared_ptr<ImageMergeAction> Action;

    std::shared_ptr<Image> TargetImage;
    std::vector<std::shared_ptr<Image>> MergedImages;

    // Widgets
    Gtk::Label TargetLabel;
    SuperViewer TargetImageViewer;
    SuperContainer MergedImageContainer;
    Gtk::Button RemoveSelected;
};

} // namespace DV
