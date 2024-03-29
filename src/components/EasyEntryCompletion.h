#pragma once

#include "IsAlive.h"

#include <string>

#include <gtkmm.h>

namespace DV {

//! \brief Provides suggestions when typing into a Gtk::Entry
class EasyEntryCompletion : public IsAlive {

    class CompletionColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        CompletionColumns()
        {
            add(m_tag_text);
        }

        Gtk::TreeModelColumn<Glib::ustring> m_tag_text;
    };

public:
    explicit EasyEntryCompletion(size_t suggestionstoshow = 50, size_t mincharsbeforecomplete = 3);
    ~EasyEntryCompletion();

    //! \brief Call this to setup this object to show suggestions on entry
    //! \param onselected Called when an entry is selected with the suggestion text,
    //! if returns true the entry is accepted and the current text is cleared. Passing in
    //! null here will use the default Gtk action for the completion
    void Init(Gtk::Entry* entry, std::function<bool(const Glib::ustring& str)> onselected,
        std::function<std::vector<std::string>(const std::string& str, size_t max)> getsuggestions);


protected:
    //! \brief Called when an autocomplete entry is selected. Calls the OnSelected callback
    bool _OnMatchSelected(const Gtk::TreeModel::iterator& iter);

    //! \brief Updates the suggestions
    void _OnTextUpdated();

    //! \brief For completing any part
    bool DoesCompletionMatch(
        const Glib::ustring& key, const Gtk::TreeModel::const_iterator& iter);

protected:
    const size_t SuggestionsToShow;
    const size_t CompleteAfterCharacters;

    std::function<bool(const Glib::ustring& str)> OnSelected;

    //! Called to get the completion data
    std::function<std::vector<std::string>(const std::string& str, size_t max)> GetSuggestions;

    Gtk::Entry* EntryWithSuggestions = nullptr;

    // Suggestion data
    Glib::RefPtr<Gtk::EntryCompletion> Completion;
    CompletionColumns CompletionColumnTypes;
    Glib::RefPtr<Gtk::ListStore> CompletionRows;
};


} // namespace DV
