#pragma once

#include "core/Plugin.h"
#include "core/CacheManager.h"

#include "Image.h"

namespace DV{

class MemoryDLJob;

//! \brief Loads an image from memory to be displayed
class DownloadLoadedImage : public LoadedImage{
public:

    //! \param thumb If true will resize the image after loading
    DownloadLoadedImage(bool thumb);

    //! When download fails
    void OnFail(std::shared_ptr<DownloadLoadedImage> thisobject, const std::string &error =
        "HTTP request failed");

    //! \brief Called when download succeeds, this should queue a worker task to load
    //! the image
    void OnSuccess(std::shared_ptr<DownloadLoadedImage> thisobject, const std::string &data);
    
protected:
    
    bool Thumb;
};

//! \brief Image that is loaded from an URL. Can be used pretty much the same,
//! but cannot be imported to the database
class InternetImage : public Image{
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

    //! \brief Returns a loaded image object that will download this image
    virtual std::shared_ptr<LoadedImage> GetImage() override;
    
    //! \brief Returns a loaded image object that will download this image and then scale it
    //! \todo Add support for downloading premade thumbnails
    virtual std::shared_ptr<LoadedImage> GetThumbnail() override;

    //! \brief Returns the local filename once this has been downloaded
    //! \note The file will be in the staging folder
    //! \warning This hashes the DLURL each time this is called
    std::string GetLocalFilename() const;

protected:

    //! \brief Starts downloading the file if not already downloading
    void _CheckFileDownload();

protected:

    //! Download URL for the full image
    std::string DLURL;


    //! Referrer to use when downloading
    std::string Referrer;

    //! The downloaded image file, can be written to a file and added to the database if wanted
    std::shared_ptr<MemoryDLJob> FileDL;
    bool DLReady = false;

    std::shared_ptr<DownloadLoadedImage> FullImage;
    std::shared_ptr<DownloadLoadedImage> ThumbImage;

};

}
