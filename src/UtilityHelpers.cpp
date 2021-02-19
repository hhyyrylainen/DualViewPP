// ------------------------------------ //
#include "UtilityHelpers.h"

#include "resources/Tags.h"

#include "Exceptions.h"
#include "Common/StringOperations.h"

#include <optional>
#include <string_view>

using namespace DV;
// ------------------------------------ //

class LocaleHolder {
public:
    LocaleHolder()
    {
        boost::locale::generator gen;
        locale = gen("");
    }

    std::locale locale;
};

static LocaleHolder CreatedLocale;

std::string DV::StringToLower(const std::string& str)
{
    return boost::locale::to_lower(str, CreatedLocale.locale);
}
// ------------------------------------ //
bool DV::CompareSuggestionStrings(
    const std::string& str, const std::string& leftInput, const std::string& rightInput)
{
    const auto left = StringToLower(leftInput);
    const auto right = StringToLower(rightInput);

    // Safety check to guarantee self comparisons
    if(left == right)
        return false;

    if(left == str && (right != str)) {

        // Exact match first //
        return true;
    }

    if(right == str && (left != str)) {

        // Other is exact match //
        return false;
    }

    if(Leviathan::StringOperations::StringStartsWith(left, str) &&
        !Leviathan::StringOperations::StringStartsWith(right, str)) {
        // Matching prefix with pattern first
        return true;
    }

    // Check is other before //
    if(Leviathan::StringOperations::StringStartsWith(right, str) &&
        !Leviathan::StringOperations::StringStartsWith(left, str)) {
        return false;
    }

    // We need to cast to signed integers to not overflow and mess up these length calculations
    const auto leftLen = static_cast<int64_t>(left.length());
    const auto rightLen = static_cast<int64_t>(right.length());
    const auto strLen = static_cast<int64_t>(str.length());

    // Sort which one is closer in length to str first
    if(std::abs(strLen - leftLen) < std::abs(strLen - rightLen)) {
        // Closer in length to user input
        return true;
    }

    if(std::abs(strLen - rightLen) < std::abs(strLen - leftLen)) {
        // Other is closer
        return false;
    }

    // Normal alphabetical order
    return left < right;
}

bool DV::CompareSuggestionTags(const std::string& str, const std::shared_ptr<Tag>& left,
    const std::shared_ptr<Tag>& right)
{
    return CompareSuggestionStrings(str, left->GetName(), right->GetName());
}
// ------------------------------------ //
std::optional<long> ParseEndingNumber(const std::string& str, size_t& numberEnd){
    if(str.empty())
        return {};

    size_t i = str.size() - 1;

    while (true){
        if(std::isdigit(str[i])){
            // Guard against first character being a digit
            if(i > 0)
                --i;
        } else {
            // Non digit found
            ++i;
            break;
        }

        if(i == 0)
            break;
    }

    numberEnd = i;

    if(i >= str.size()){
        // No number found
        return {};
    }

    try {
        return std::strtol(str.data() + i, nullptr, 10);
    } catch(...) {
        // Not a number
        return {};
    }
}

bool DV::CompareFilePaths(const std::string& left, const std::string& right)
{
    if(left == right)
        return false;

    // Parse the paths and extensions from the names
    // TODO: could allocating this many strings be avoided
    const auto leftPath = Leviathan::StringOperations::GetPath(left);
    const auto plainLeft = Leviathan::StringOperations::RemoveExtension(left);

    const auto rightPath = Leviathan::StringOperations::GetPath(left);
    const auto plainRight = Leviathan::StringOperations::RemoveExtension(right);

    // Still compare folder names when paths are different length
    if(leftPath.length() < rightPath.length())
        return left < right;

    if(leftPath.length() > rightPath.length())
        return left < right;

    // Detect if they end in numbers
    size_t leftEnd;
    const auto leftNumber = ParseEndingNumber(plainLeft, leftEnd);
    size_t rightEnd;
    const auto rightNumber = ParseEndingNumber(plainRight, rightEnd);

    if(leftNumber && rightNumber) {
        // Check that prefixes are the same
        bool prefixesMatch = false;
        if(leftEnd == rightEnd) {

            prefixesMatch = true;

            // TODO: could maybe use string_view here
            for(size_t i = 0; i < leftEnd && i < plainLeft.length(); ++i) {
                if(plainLeft[i] != plainRight[i]) {
                    prefixesMatch = false;
                    break;
                }
            }
        }

        if(prefixesMatch) {
            // Compare actual numbers
            if(leftNumber < rightNumber)
                return true;

            if(rightNumber < leftNumber)
                return false;
        } else {
            // Compare the prefixes for sorting
            const std::string_view leftPrefix{plainLeft.data(), leftEnd};
            const std::string_view rightPrefix{plainRight.data(), rightEnd};

            if(leftPrefix < rightPrefix)
                return true;

            if(rightPrefix < leftPrefix)
                return false;

            // If there is no ordering for the prefixes fall through to the basic compare
        }
    }

    // Fallback to basic comparison
    return left < right;
}
// ------------------------------------ //
ResourceDataHolder DV::LoadResource(const std::string& name)
{
    try {
        auto data = Gio::Resource::lookup_data_global(name);

        if(!data)
            throw Leviathan::NotFound("resource not found");

        gsize size;
        const auto* ptr = data->get_data(size);

        if(!ptr)
            throw Leviathan::NotFound("resource has invalid ptr");

        return {data, std::string_view(reinterpret_cast<const char*>(ptr), size)};

    } catch(const Glib::Error&) {
        throw Leviathan::NotFound("resource not found");
    }
}

std::string DV::LoadResourceCopy(const std::string& name)
{
    auto resource = LoadResource(name);
    return std::string(resource.DataAsStr);
}
