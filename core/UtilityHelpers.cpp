// ------------------------------------ //
#include "UtilityHelpers.h"

using namespace DV;
// ------------------------------------ //
bool DV::CompareSuggestionStrings(const std::string &str,
    const std::string &left, const std::string &right)
{
    // Safety check to guarantee self comparisons
    if(left == right)
        return false;
            
    if(left == str && (right != str)){

        // Exact match first //
        return true;
    }
    
    if(Leviathan::StringOperations::StringStartsWith(left, str) && 
        !Leviathan::StringOperations::StringStartsWith(right, str))
    {
        // Matching prefix with pattern first
        return true;
    }

    // Check is other before //
    if(Leviathan::StringOperations::StringStartsWith(right, str) && 
        !Leviathan::StringOperations::StringStartsWith(left, str))
    {
        return false;
    }    
            
    // Sort which one is closer in length to str first
    if(std::abs(str.length() - left.length()) <
        std::abs(str.length() - right.length()))
    {
        // Closer in length to user input
        return true;
    }

    // Normal alphabetical order
    return left < right;
}
