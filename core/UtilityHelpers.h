#pragma once

#include "leviathan/Common/StringOperations.h"

#include <algorithm>
#include <string>
#include <boost/locale.hpp>


namespace DV{

class Tag;



//! \brief Converts a unicode string to lower
//! \todo Cache the boost::locale::generator or make this otherwise more efficient
std::string StringToLower(const std::string &str);

//! \brief Sort function for SortSuggestions
bool CompareSuggestionStrings(const std::string &str,
    const std::string &left, const std::string &right);

//! \brief Tag Sort function
bool CompareSuggestionTags(const std::string &str,
    const std::shared_ptr<Tag> &left, const std::shared_ptr<Tag> &right);

//! \brief Sorts suggestions for str
template<class IterType>
    void SortSuggestions(IterType begin, IterType end, const std::string &str)
{
    std::sort(begin, end, std::bind(&DV::CompareSuggestionStrings, StringToLower(str),
            std::placeholders::_1, std::placeholders::_2));
}

template<class IterType>
    void SortTagSuggestions(IterType begin, IterType end, const std::string &str)
{
    std::sort(begin, end, std::bind(&DV::CompareSuggestionTags, StringToLower(str),
            std::placeholders::_1, std::placeholders::_2));
}



}
