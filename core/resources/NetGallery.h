#pragma once

#include "DatabaseResource.h"

#include "core/VirtualPath.h"

namespace DV{

class PreparedStatement;

class InternetImage;

//! \brief A file belonging to a NetGallery
class NetFile : public DatabaseResource{
public:

    //! \brief Constructor for creating new ones
    NetFile(const std::string &url, const std::string &referrer, const std::string &name,
        const std::string &tagstr = "");

    //! \brief Constructor for database loading
    NetFile(Database &db, Lock &dblock, PreparedStatement &statement, int64_t id);

    auto GetFileURL() const{

        return FileURL;
    }

    auto GetPageReferrer() const{

        return PageReferrer;
    }

    auto GetPreferredName() const{

        return PreferredName;
    }
    
    auto GetTagsString() const{

        return TagsString;
    }
    

protected:

    void _DoSave(Database &db) override;

protected:

    
    std::string FileURL;
    std::string PageReferrer;
    std::string PreferredName;
    std::string TagsString;
};

//! \brief Gallery that contains URL addresses of images
//!
//! Can be downloaded with DownloadManager
class NetGallery : public DatabaseResource{
public:

    //! \brief Constructor for creating new ones
    NetGallery(const std::string &url);

    //! \brief Constructor for database loading
    NetGallery(Database &db, Lock &dblock, PreparedStatement &statement, int64_t id);
    
    ~NetGallery();

    auto GetGalleryUrl() const{

        return GalleryUrl;
    }

    auto GetTargetPath() const{

        return TargetPath;
    }

    auto GetTargetGalleryName() const{

        return TargetGalleryName;
    }

    auto GetCurrentlyScanned() const{

        return CurrentlyScanned;
    }

    auto GetIsDownloaded() const{

        return IsDownloaded;
    }

    auto GetTagsString() const{

        return TagsString;
    }

    void SetIsDownload(bool newdownloaded){

        IsDownloaded = newdownloaded;
        OnMarkDirty();
    }

    void SetTags(const std::string &str){

        TagsString = str;
        OnMarkDirty();
    }

    void SetTargetPath(const VirtualPath &path){

        if(path.IsRootPath()){

            TargetPath = "";
            OnMarkDirty();
            return;
        }

        TargetPath = path;
        OnMarkDirty();
    }

    //! \brief Adds all images to this gallery
    //! \note Doesn't check for duplicates
    void AddFilesToDownload(const std::vector<std::shared_ptr<InternetImage>> &images);
    
protected:

    void _DoSave(Database &db) override;

protected:


    std::string GalleryUrl;

    std::string TargetPath;
    
    std::string TargetGalleryName; 

    //! \unused
    std::string CurrentlyScanned;

    bool IsDownloaded;

    std::string TagsString;
};

};


