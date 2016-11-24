// ------------------------------------ //
#include "EasyEntryCompletion.h"

#include "core/DualView.h"

#include "Common.h"

#include "leviathan/Common/StringOperations.h"

using namespace DV;
// ------------------------------------ //

EasyEntryCompletion::EasyEntryCompletion(size_t suggestionstoshow /*= 50*/,
    size_t mincharsbeforecomplete /*= 3*/) :
    SuggestionsToShow(suggestionstoshow), CompleteAfterCharacters(mincharsbeforecomplete)
{

}

EasyEntryCompletion::~EasyEntryCompletion(){

    
}
// ------------------------------------ //
void EasyEntryCompletion::Init(Gtk::Entry* entry,
    std::function<bool (const Glib::ustring &str)> onselected,
    std::function<std::vector<std::string> (std::string str, size_t max)> getsuggestions)
{
    EntryWithSuggestions = entry;

    LEVIATHAN_ASSERT(EntryWithSuggestions, "EasyEntryCompletion: got nullptr entry");

    GetSuggestions = getsuggestions;

    LEVIATHAN_ASSERT(EntryWithSuggestions, "EasyEntryCompletion: got null suggestion "
        "retrieve function");

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

    // Set match function //
    Completion->set_match_func(sigc::mem_fun(*this,
            &EasyEntryCompletion::DoesCompletionMatch));
    
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
    if(EntryWithSuggestions->get_text_length() < CompleteAfterCharacters)
        return;    

    auto isalive = GetAliveMarker();
    auto text = EntryWithSuggestions->get_text();

    DualView::Get().QueueDBThreadFunction([this, isalive, text, suggest { GetSuggestions },
            count { SuggestionsToShow }]()
        {
            const std::string str = DualView::StringToLower(text);
            
            auto result = suggest(str, count);

            // Sort the result //
            std::sort(result.begin(), result.end(), [&str](
                    const std::string &left, const std::string &right) -> bool
                {
                    if(left == str && (right != str)){

                        // Exact match first //
                        return true;
                    }
            
                    if(Leviathan::StringOperations::StringStartsWith(
                            DualView::StringToLower(left), str) && 
                        !Leviathan::StringOperations::StringStartsWith(
                            DualView::StringToLower(right), str))
                    {
                        // Matching prefix with pattern first
                        return true;
                    }

                    // Sort which one is closer in length to str first
                    return std::abs(str.length() - left.length()) <
                        std::abs(str.length() - right.length());
                });

            
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
// ------------------------------------ //
bool EasyEntryCompletion::DoesCompletionMatch(const Glib::ustring& key,
    const Gtk::TreeModel::const_iterator& iter)
{
    Gtk::TreeModel::Row row = *(iter);
    return static_cast<Glib::ustring>(row[
            CompletionColumnTypes.m_tag_text]).lowercase().find(key.lowercase())
        != Glib::ustring::npos;
}


