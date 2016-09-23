#pragma once

#include <string>
#include <chrono>

namespace DV{


class Collection{
public:

    //! \brief Creates a non-database saved collection
    Collection(const std::string &name);


    //! \brief Return Name with illegal characters replaced with spaces
    std::string GetNameForFolder() const;

protected:

    std::string Name;

    std::chrono::system_clock::time_point AddDate = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point ModifyDate = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point LastView = std::chrono::system_clock::now();

    bool IsPrivate = false;
};

}
