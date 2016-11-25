#pragma once

#include "leviathan/Common/StringOperations.h"

#include <algorithm>
#include <string>


namespace DV{

//! \brief Sort function for SortSuggestions
bool CompareSuggestionStrings(const std::string &str,
    const std::string &left, const std::string &right);

//! \brief Sorts suggestions for str
template<class IterType>
    void SortSuggestions(IterType begin, IterType end, const std::string &str)
{
    std::sort(begin, end, std::bind(&DV::CompareSuggestionStrings, str,
            std::placeholders::_1, std::placeholders::_2));
}



}
