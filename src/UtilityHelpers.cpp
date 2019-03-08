// ------------------------------------ //
#include "UtilityHelpers.h"

#include "resources/Tags.h"

#include "Exceptions.h"

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
