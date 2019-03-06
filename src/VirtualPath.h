#pragma once

#include <limits>
#include <string>

namespace DV {

//! \brief Represents a virtual path
//! \note Any path that needs to be used to retrieve folders or collections must begin
//! with the root path
class VirtualPath {
public:
    struct iterator {
        //! \param end If true creates the end iterator
        iterator(const VirtualPath& path, bool end = false);
        iterator(const iterator& other);
        ~iterator() = default;

        iterator& operator=(const iterator& other);
        bool operator==(const iterator& other) const;
        bool operator!=(const iterator& other) const;

        iterator& operator++(); // prefix increment
        iterator& operator--();

        std::string operator*() const;

    protected:
        std::string::const_iterator Path;
        std::string::const_iterator Begin;
        std::string::const_iterator End;
    };

public:
    //! Root path
    VirtualPath();

    //! \brief Creates a path from the string
    //! \param addroot If true makes sure that the path begins with the root path
    //! \exception Leviathan::InvalidArgument if the path is invalid
    //! \todo Exception if there are .. path elements
    VirtualPath(const std::string& path, bool addroot = false);

    ~VirtualPath() = default;

    //! \brief Moves this path to point to the folder one level up from the current one
    //! \note If called on the root path this will still point to the root path
    void MoveUpOneFolder();

    //! \brief Adds second to the end of the first path
    //! \param second The path to append to first, if begins with root second will be returned
    //! as is
    static VirtualPath Combine(const VirtualPath& first, const VirtualPath& second);

    const std::string& GetPathString() const
    {
        return PathStr;
    }

    //! \brief Returns true if this is the path to root
    bool IsRootPath() const
    {
        if(PathStr == "Root" || PathStr == "Root/")
            return true;
        return false;
    }

    // Operators //

    //! \brief Combines paths
    VirtualPath operator/(const VirtualPath& other) const
    {
        return Combine(*this, other);
    }

    //! \brief Moves up one folder
    VirtualPath& operator--()
    {
        MoveUpOneFolder();
        return *this;
    }

    //! \brief Returns true if the paths are exactly the same
    //! \warning Two different paths can point to the same thing because folders
    //! can be inside multiple folders
    bool operator==(const VirtualPath& other) const
    {
        return PathStr == other.PathStr;
    }

    operator std::string() const
    {
        return PathStr;
    }

    // Iterators for path parts
    iterator begin() const;

    iterator end() const;

protected:
    std::string PathStr = "Root/";
};

//! Pretty printing for paths
std::ostream& operator<<(std::ostream& stream, const VirtualPath& value);

} // namespace DV
