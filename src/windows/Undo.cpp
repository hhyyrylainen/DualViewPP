// ------------------------------------ //
#include "Undo.h"

#include "resources/DatabaseAction.h"

#include "Database.h"
#include "DualView.h"

using namespace DV;
// ------------------------------------ //
UndoWindow::UndoWindow() :
    ClearHistory("Clear History"), HistorySizeLabel("History items to keep"),
    MainContainer(Gtk::ORIENTATION_VERTICAL), ListContainer(Gtk::ORIENTATION_VERTICAL),
    NothingToShow("No history items available")
{
    signal_delete_event().connect(sigc::mem_fun(*this, &BaseWindow::_OnClosed));
    add_events(Gdk::KEY_PRESS_MASK);
    signal_key_press_event().connect(
        sigc::mem_fun(*this, &UndoWindow::_StartSearchFromKeypress));

    auto accelGroup = Gtk::AccelGroup::create();

    set_default_size(500, 300);
    property_resizable() = true;

    Menu.set_image_from_icon_name("open-menu-symbolic");

    // Window specific controls
    ClearHistory.property_relief() = Gtk::RELIEF_NONE;
    MenuPopover.Container.pack_start(ClearHistory);
    MenuPopover.Container.pack_start(Separator1);
    MenuPopover.Container.pack_start(HistorySizeLabel);

    HistorySize.property_editable() = true;
    HistorySize.property_input_purpose() = Gtk::INPUT_PURPOSE_NUMBER;
    HistorySize.property_snap_to_ticks() = true;
    HistorySize.property_snap_to_ticks() = true;
    HistorySize.set_range(1, 250);
    HistorySize.set_increments(1, 10);
    HistorySize.set_digits(0);
    // TODO: implement logic
    HistorySize.set_value(50);

    MenuPopover.Container.pack_start(HistorySize);

    MenuPopover.show_all_children();

    Menu.set_popover(MenuPopover);

    SearchButton.set_image_from_icon_name("edit-find-symbolic");
    SearchButton.add_accelerator("clicked", accelGroup, GDK_KEY_f,
        Gdk::ModifierType::CONTROL_MASK, Gtk::AccelFlags::ACCEL_VISIBLE);

    HeaderBar.property_title() = "Latest Actions";
    HeaderBar.property_show_close_button() = true;
    HeaderBar.pack_end(Menu);
    HeaderBar.pack_end(SearchButton);
    set_titlebar(HeaderBar);

    //
    // Content area
    //
    MainContainer.property_vexpand() = true;
    MainContainer.property_hexpand() = true;


    Search.signal_search_changed().connect(sigc::mem_fun(*this, &UndoWindow::_SearchUpdated));

    SearchBar.property_search_mode_enabled() = false;

    SearchActiveBinding = Glib::Binding::bind_property(SearchButton.property_active(),
        SearchBar.property_search_mode_enabled(), Glib::BINDING_BIDIRECTIONAL);
    SearchBar.add(Search);
    MainContainer.add(SearchBar);


    QueryingDatabase.property_active() = true;
    MainArea.add_overlay(QueryingDatabase);


    ListScroll.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_ALWAYS);
    ListScroll.property_vexpand() = true;
    ListScroll.property_hexpand() = true;

    ListContainer.property_vexpand() = true;
    ListContainer.property_hexpand() = true;

    NothingToShow.property_halign() = Gtk::ALIGN_CENTER;
    NothingToShow.property_hexpand() = true;
    NothingToShow.property_valign() = Gtk::ALIGN_CENTER;
    NothingToShow.property_vexpand() = true;

    ListContainer.set_spacing(4);
    ListContainer.property_margin_top() = 4;
    ListContainer.property_margin_bottom() = 4;
    ListContainer.property_margin_start() = 4;
    ListContainer.property_margin_end() = 4;

    ListContainer.add(NothingToShow);
    ListScroll.add(ListContainer);

    MainArea.add(ListScroll);

    MainContainer.add(MainArea);

    add(MainContainer);

    add_accel_group(accelGroup);

    show_all_children();

    _SearchUpdated();
}

UndoWindow::~UndoWindow()
{
    Close();
}

void UndoWindow::_OnClose() {}
// ------------------------------------ //
bool UndoWindow::_StartSearchFromKeypress(GdkEventKey* event)
{
    return SearchBar.handle_event(event);
}
// ------------------------------------ //
void UndoWindow::Clear()
{
    while(!FoundActions.empty()) {

        ListContainer.remove(*FoundActions.back());
        FoundActions.pop_back();
    }
}
// ------------------------------------ //
void UndoWindow::_SearchUpdated()
{
    NothingToShow.property_visible() = false;
    QueryingDatabase.property_visible() = true;

    const std::string search = Search.property_text().get_value();

    auto isalive = GetAliveMarker();

    DualView::Get().QueueDBThreadFunction([=]() {
        auto actions = DualView::Get().GetDatabase().SelectLatestDatabaseActions(search);

        DualView::Get().InvokeFunction([this, isalive, actions{std::move(actions)}]() {
            INVOKE_CHECK_ALIVE_MARKER(isalive);

            _FinishedQueryingDB(actions);
        });
    });
}
// ------------------------------------ //
void UndoWindow::_FinishedQueryingDB(
    const std::vector<std::shared_ptr<DatabaseAction>>& actions)
{
    QueryingDatabase.property_visible() = false;

    Clear();

    for(const auto& action : actions) {

        FoundActions.push_back(std::make_shared<ActionDisplay>(action));
        ListContainer.add(*FoundActions.back());
        FoundActions.back()->show();
    }

    if(FoundActions.empty())
        NothingToShow.property_visible() = true;
}
// ------------------------------------ //
// ActionDisplay
ActionDisplay::ActionDisplay(const std::shared_ptr<DatabaseAction>& action) :
    Action(action), MainBox(Gtk::ORIENTATION_HORIZONTAL), LeftSide(Gtk::ORIENTATION_VERTICAL),
    RightSide(Gtk::ORIENTATION_HORIZONTAL), UndoRedoButton("Undo"), EditButton("Edit")
{
    if(!Action)
        throw InvalidState("given nullptr for action");

    // The description generation accesses the database so we do that in the background
    Description.property_halign() = Gtk::ALIGN_START;
    Description.property_valign() = Gtk::ALIGN_START;
    // Description.property_margin_start() = 8;
    Description.property_margin_top() = 3;
    Description.property_label() =
        "Loading description for action " + std::to_string(Action->GetID());

    Description.property_max_width_chars() = 80;
    LeftSide.pack_start(Description, false, false);

    ResourcePreviews.property_vexpand() = true;
    ResourcePreviews.set_min_content_width(140);
    ResourcePreviews.set_min_content_height(80);
    ResourcePreviews.SetItemSize(LIST_ITEM_SIZE::TINY);
    ContainerFrame.add(ResourcePreviews);
    LeftSide.pack_end(ContainerFrame, true, true);

    MainBox.pack_start(LeftSide, true, true);

    UndoRedoButton.property_valign() = Gtk::ALIGN_CENTER;
    UndoRedoButton.property_halign() = Gtk::ALIGN_CENTER;
    UndoRedoButton.property_sensitive() = false;
    RightSide.pack_start(UndoRedoButton, false, false);

    EditButton.property_valign() = Gtk::ALIGN_CENTER;
    EditButton.property_halign() = Gtk::ALIGN_CENTER;
    EditButton.property_sensitive() = false;
    RightSide.pack_start(EditButton, false, false);
    RightSide.set_homogeneous(true);
    RightSide.set_spacing(2);

    MainBox.pack_end(RightSide, false, false);
    MainBox.set_spacing(3);

    add(MainBox);

    show_all_children();

    RefreshData();
}

ActionDisplay::~ActionDisplay() {}
// ------------------------------------ //
void ActionDisplay::RefreshData()
{
    auto isalive = GetAliveMarker();

    DualView::Get().QueueDBThreadFunction([action = this->Action, this, isalive]() {
        auto description = action->GenerateDescription();

        auto previewItems = action->LoadPreviewItems(10);

        DualView::Get().InvokeFunction(
            [this, isalive, description, previewItems = std::move(previewItems)]() {
                INVOKE_CHECK_ALIVE_MARKER(isalive);


                _OnDataRetrieved(description, previewItems);
            });
    });
}

void ActionDisplay::_OnDataRetrieved(const std::string& description,
    const std::vector<std::shared_ptr<ResourceWithPreview>>& previewitems)
{
    DualView::IsOnMainThreadAssert();

    Description.property_label() = description;

    if(previewitems.empty()) {

        ContainerFrame.property_visible() = false;
        ResourcePreviews.Clear();

    } else {

        ContainerFrame.property_visible() = true;
        ResourcePreviews.SetShownItems(previewitems.begin(), previewitems.end());
    }
}
