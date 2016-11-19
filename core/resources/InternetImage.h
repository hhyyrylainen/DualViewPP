#pragma once

#include "core/Plugin.h"

#include "Image.h"

namespace DV{

class InternetImage : public Image{
public:

protected:

    //! \brief Loads url and (future) tags from link
    InternetImage(const ScanFoundImage &link);

    //! Called by Create functions
    void Init();
    
public:

    //! \brief Loads a database image
    //! \exception Leviathan::InvalidArgument if link doesn't have filename
    inline static auto Create(const ScanFoundImage &link){
        
        auto obj = std::shared_ptr<InternetImage>(new InternetImage(link));
        obj->Init();
        return obj;
    }


    //! \brief Returns the local filename once this has been downloaded
    //! \note The file will be in the staging folder
    //! \warning This hashes the DLURL each time this is called
    std::string GetLocalFilename() const;

protected:

protected:

    //! Download URL for the full image
    std::string DLURL;

    //! Referrer to use when downloading
    std::string Referrer;

    //! State of the download
    
};

}
