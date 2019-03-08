#pragma once

#include "Common/StringOperations.h"

#include <boost/locale.hpp>

#include <giomm/resource.h>

#include <algorithm>
#include <string>
#include <string_view>


namespace DV {

class Tag;



//! \brief Converts a unicode string to lower
//! \todo Cache the boost::locale::generator or make this otherwise more efficient
std::string StringToLower(const std::string& str);

//! \brief Sort function for SortSuggestions
bool CompareSuggestionStrings(
    const std::string& str, const std::string& left, const std::string& right);

//! \brief Tag Sort function
bool CompareSuggestionTags(const std::string& str, const std::shared_ptr<Tag>& left,
    const std::shared_ptr<Tag>& right);

//! \brief Sorts suggestions for str
template<class IterType>
void SortSuggestions(IterType begin, IterType end, const std::string& str)
{
    std::sort(begin, end,
        std::bind(&DV::CompareSuggestionStrings, StringToLower(str), std::placeholders::_1,
            std::placeholders::_2));
}

template<class IterType>
void SortTagSuggestions(IterType begin, IterType end, const std::string& str)
{
    std::sort(begin, end,
        std::bind(&DV::CompareSuggestionTags, StringToLower(str), std::placeholders::_1,
            std::placeholders::_2));
}

//! This is used as decompressed data might be deleted before we are done with it
struct ResourceDataHolder {
    Glib::RefPtr<const Glib::Bytes> Data;
    std::string_view DataAsStr;
};

//! \brief Loads an std::string_view from a resource
//! \exception Leviathan::NotFound if not found
ResourceDataHolder LoadResource(const std::string& name);

//! \brief Loads an std::string from a resource
//! \note This makes a copy of the data
//! \exception Leviathan::NotFound if not found
std::string LoadResourceCopy(const std::string& name);


} // namespace DV
