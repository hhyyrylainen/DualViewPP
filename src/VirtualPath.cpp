// ------------------------------------ //
#include "VirtualPath.h"

#include "Common/StringOperations.h"

using namespace DV;
// ------------------------------------ //

VirtualPath::VirtualPath() : PathStr("Root/") {}

VirtualPath::VirtualPath(const std::string& path, bool addroot /*= false*/)
{
    PathStr =
        Leviathan::StringOperations::ReplaceSingleCharacter<std::string>(path, '\\', '/');
    Leviathan::StringOperations::RemovePreceedingTrailingSpaces(PathStr);

    if(addroot) {

        if(!Leviathan::StringOperations::StringStartsWith<std::string>(PathStr, "Root/")) {

            PathStr = (VirtualPath() / *this).GetPathString();
        }
    }
}
// ------------------------------------ //
void VirtualPath::MoveUpOneFolder()
{
    if(PathStr == "Root/" || PathStr.empty()) {

        PathStr = "Root/";
        return;
    }

    // Scan backward for the previous '/' //
    size_t cutEnd = PathStr.size() - 2;
    for(; cutEnd < PathStr.size(); --cutEnd) {

        if(PathStr[cutEnd] == '/')
            break;
    }

    if(cutEnd >= PathStr.size()) {

        // No folder to go up to
        return;
    }

    PathStr = PathStr.substr(0, cutEnd + 1);
}
// ------------------------------------ //
VirtualPath VirtualPath::Combine(const VirtualPath& first, const VirtualPath& second)
{
    if(Leviathan::StringOperations::StringStartsWith<std::string>(second.PathStr, "Root/"))
        return second;

    // Combining fails if both are empty //
    if(first.PathStr.empty() && second.PathStr.empty()) {

        return VirtualPath("");
    }

    // Check for empty paths //
    if(first.PathStr.empty()) {

        return second;
    }

    if(second.PathStr.empty()) {

        return first;
    }

    // We need to actually combine something //
    std::string newPath = first.PathStr;

    if(newPath.back() != '/' && second.PathStr.front() != '/')
        newPath += '/';

    // Remove duplicate '/'
    if(newPath.back() == '/' && second.PathStr.front() == '/')
        newPath.pop_back();

    return VirtualPath(newPath + second.PathStr);
}
// ------------------------------------ //
VirtualPath::iterator VirtualPath::begin() const
{
    return iterator(*this, false);
}

VirtualPath::iterator VirtualPath::end() const
{
    return iterator(*this, true);
}
// ------------------------------------ //
std::ostream& DV::operator<<(std::ostream& stream, const DV::VirtualPath& value)
{
    stream << value.GetPathString();
    return stream;
}

// ------------------------------------ //
// iterator
//! \param end If true creates the end iterator
VirtualPath::iterator::iterator(const VirtualPath& path, bool end /*= false*/) :
    Path(end ? path.PathStr.end() : path.PathStr.begin()), Begin(path.PathStr.begin()),
    End(path.PathStr.end())
{}

VirtualPath::iterator::iterator(const VirtualPath::iterator& other) :
    Path(other.Path), Begin(other.Begin), End(other.End)
{}
// ------------------------------------ //
VirtualPath::iterator& VirtualPath::iterator::operator=(const VirtualPath::iterator& other)
{
    Path = other.Path;
    End = other.End;

    return *this;
}

bool VirtualPath::iterator::operator==(const VirtualPath::iterator& other) const
{
    if(Path >= End && other.Path >= other.End)
        return true;

    return Path == other.Path;
}

bool VirtualPath::iterator::operator!=(const VirtualPath::iterator& other) const
{
    return !(*this == other);
}

VirtualPath::iterator& VirtualPath::iterator::operator++()
{
    ++Path;

    if(Path >= End) {
        Path = End;
        return *this;
    }

    // Loop forward while character isn't '/'
    while(Path != End && (*Path) != '/') {

        ++Path;
    }

    // Move over the '/'
    ++Path;

    return *this;
}

VirtualPath::iterator& VirtualPath::iterator::operator--()
{
    --Path;

    if(Path == End)
        return *this;

    if(Path < Begin) {
        Path = End;
        return *this;
    }

    // Loop back while character isn't '/'
    while(Path != End && (*Path) != '/') {

        --Path;
    }

    // Move to before the '/'
    --Path;

    return *this;
}

std::string VirtualPath::iterator::operator*() const
{
    std::string pathComponent;

    for(auto iter = Path; iter < End; ++iter) {

        if(*iter == '/')
            break;

        pathComponent.push_back(*iter);
    }

    return pathComponent;
}
