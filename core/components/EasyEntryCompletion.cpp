// ------------------------------------ //
#include "EasyEntryCompletion.h"

#include "core/DualView.h"

#include "Common.h"

using namespace DV;
// ------------------------------------ //

EasyEntryCompletion::EasyEntryCompletion(size_t suggestionstoshow /*= 50*/,
    size_t mincharsbeforecomplete /*= 3*/) :
    SuggestionsToShow(suggestionstoshow), CompleteAfterCharacthers(mincharsbeforecomplete)
{

}

EasyEntryCompletion::~EasyEntryCompletion(){

    
}
// ------------------------------------ //
void EasyEntryCompletion::Init(Gtk::Entry* entry,
    std::function<bool (const Glib::ustring &str)> onselected)
{
    EntryWithSuggestions = entry;

    LEVIATHAN_ASSERT(EntryWithSuggestions, "EasyEntryCompletion: got nullptr entry");

    OnSelected = onselected;
    
    Completion = Gtk::EntryCompletion::create();
    
    EntryWithSuggestions->set_completion(Completion);

    // Create an empty liststore for completions
    CompletionRows = Gtk::ListStore::create(CompletionColumnTypes);
    Completion->set_model(CompletionRows);

    // Hookup callback
    if(OnSelected)
        Completion->signal_match_selected().connect(sigc::mem_fun(*this,
                &EasyEntryCompletion::_OnMatchSelected), false);

    Completion->set_text_column(CompletionColumnTypes.m_tag_text);

    // Doesn't seem to work
    //TagCompletion->set_inline_completion();
    // This messes with auto completion
    //TagCompletion->set_inline_selection();

    
    EntryWithSuggestions->signal_changed().connect(sigc::mem_fun(*this,
            &EasyEntryCompletion::_OnTextUpdated));
}
// ------------------------------------ //
bool EasyEntryCompletion::_OnMatchSelected(const Gtk::TreeModel::iterator &iter){

    if(!OnSelected)
        return false;

    Gtk::TreeModel::Row row = *(iter);
    const bool remove = OnSelected(static_cast<Glib::ustring>(row[
                CompletionColumnTypes.m_tag_text]));

    if(remove){

        EntryWithSuggestions->set_text("");
    }
    
    return true;
}
// ------------------------------------ //
void EasyEntryCompletion::_OnTextUpdated(){

    // No completion if less than 3 characters
    if(EntryWithSuggestions->get_text_length() < 3)
        return;    

    auto isalive = GetAliveMarker();
    auto text = EntryWithSuggestions->get_text();
    
    DualView::Get().QueueDBThreadFunction([this, isalive, text](){

            auto result = DualView::Get().GetSuggestionsForTag(text);
            
            DualView::Get().InvokeFunction([this, isalive, result{std::move(result)}](){

                    INVOKE_CHECK_ALIVE_MARKER(isalive);

                    CompletionRows->clear();
                    
                    // Fill the autocomplete
                    for(const auto& str : result){
                        
                        Gtk::TreeModel::Row row = *(CompletionRows->append());
                        row[CompletionColumnTypes.m_tag_text] = str;
                    }
                });
        });    
}



