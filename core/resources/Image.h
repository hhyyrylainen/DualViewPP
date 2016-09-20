#pragma once

#include <string>

namespace DV{

//! \brief Main class for all image files that are handled by DualView
//!
//! This may or may not be in the database currently. The actual image
//! data will be loaded by CacheManager
//! \note Once the image has been loaded this will be made a duplicate of
//! another image that is in the database IF the hashes match
class Image{
public:

    //! \brief Creates a non-db version of an Image.
    //! \exception Leviathan::InvalidArgument if something is wrong with the filen
    Image(const std::string &file);


private:

    
};

}
    
