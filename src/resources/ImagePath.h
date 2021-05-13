#pragma once

#include "Common.h"

#include <string>


namespace DV {

//! \brief Lightweight info about an image
class ImagePath {
public:
    ImagePath() : ID(-1) {}

    ImagePath(DBID imageId, const std::string& fullPath) : ID(imageId), Path(fullPath) {}

    bool Valid() const
    {
        return ID != -1;
    }

    DBID GetID() const
    {
        return ID;
    }

    const std::string& GetPath() const
    {
        return Path;
    }

private:
    const DBID ID;
    const std::string Path;
};

} // namespace DV
