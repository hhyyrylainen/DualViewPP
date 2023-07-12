#pragma once

#include "DatabaseResource.h"
#include "Exceptions.h"
#include "ProcessableURL.h"
#include "VirtualPath.h"

namespace DV
{
class PreparedStatement;

class InternetImage;

//! \brief A file belonging to a NetGallery
class NetFile : public DatabaseResource
{
public:
    //! \brief Constructor for creating new ones
    NetFile(
        const std::string& url, const std::string& referrer, const std::string& name, const std::string& tagstr = "");

    //! \brief Constructor for database loading
    NetFile(Database& db, DatabaseLockT& dblock, PreparedStatement& statement, int64_t id);

    ~NetFile() override;

    [[nodiscard]] auto GetFileURL() const
    {
        // When saved in the DB the canonical URL doesn't matter anymore
        return ProcessableURL(FileURL, std::string(), PageReferrer);
    }

    [[nodiscard]] const auto& GetPageReferrer() const noexcept
    {
        return PageReferrer;
    }

    [[nodiscard]] const auto& GetRawURL() const noexcept
    {
        return FileURL;
    }

    [[nodiscard]] const auto& GetPreferredName() const noexcept
    {
        return PreferredName;
    }

    [[nodiscard]] const auto& GetTagsString() const noexcept
    {
        return TagsString;
    }

protected:
    void _DoSave(Database& db) override;

protected:
    std::string FileURL;
    std::string PageReferrer;
    std::string PreferredName;
    std::string TagsString;
};

//! \brief Gallery that contains URL addresses of images
//!
//! Can be downloaded with DownloadManager
class NetGallery : public DatabaseResource
{
    friend Database;
    friend MaintenanceTools;

public:
    //! \brief Constructor for creating new ones
    NetGallery(const std::string& url, const std::string& targetgallery);

    //! \brief Constructor for database loading
    NetGallery(Database& db, DatabaseLockT& dblock, PreparedStatement& statement, int64_t id);

    ~NetGallery();

    auto GetGalleryURL() const
    {
        return GalleryURL;
    }

    auto GetTargetPath() const
    {
        return TargetPath;
    }

    auto GetTargetGalleryName() const
    {
        return TargetGalleryName;
    }

    auto GetCurrentlyScanned() const
    {
        return CurrentlyScanned;
    }

    auto GetIsDownloaded() const
    {
        return IsDownloaded;
    }

    auto GetTagsString() const
    {
        return TagsString;
    }

    bool IsDeleted() const
    {
        return Deleted;
    }

    void SetIsDownload(bool newdownloaded)
    {
        IsDownloaded = newdownloaded;
        OnMarkDirty();
    }

    void SetTags(const std::string& str)
    {
        TagsString = str;
        OnMarkDirty();
    }

    void SetTargetPath(const VirtualPath& path)
    {
        if (path.IsRootPath())
        {
            TargetPath = "";
            OnMarkDirty();
            return;
        }

        TargetPath = path;
        OnMarkDirty();
    }

    void SetTargetGalleryName(const std::string& newvalue)
    {
        TargetGalleryName = newvalue;
        OnMarkDirty();
    }

    //! \brief Adds all images to this gallery
    //! \note Doesn't check for duplicates
    void AddFilesToDownload(const std::vector<std::shared_ptr<InternetImage>>& images, DatabaseLockT& databaselock);

    //! \brief Replaces all existing items with new ones
    //! \todo Make this a reversible action
    void ReplaceItemsWith(const std::vector<std::shared_ptr<InternetImage>>& images, DatabaseLockT& databaselock);

protected:
    void _DoSave(Database& db) override;

    //! Called from Database
    void _UpdateDeletedStatus(bool deleted)
    {
        Deleted = deleted;

        GUARD_LOCK();
        NotifyAll(guard);
    }

    void ForceUnDeleteToFixMissingAction()
    {
        if (!Deleted)
            throw Leviathan::Exception("This needs to be in deleted state to call this fix missing action");

        Deleted = false;
    }

protected:
    std::string GalleryURL;

    std::string TargetPath;

    std::string TargetGalleryName = "Uncategorized";

    //! \unused
    std::string CurrentlyScanned;

    bool IsDownloaded = false;

    std::string TagsString;

    //! If true deleted from the database and everything should skip this
    bool Deleted = false;
};

}; // namespace DV
