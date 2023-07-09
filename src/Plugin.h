#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/StringOperations.h"

#include "Common.h"

namespace DV
{

/// \brief Result type for scan result combining
enum class ResultCombine : uint8_t
{
    NoNewContent = 0,
    NewResults = 1,
    NewPages = 1 << 1,
    NewContent = 1 << 2,
    NewTags = 1 << 2,
};

inline ResultCombine operator|(ResultCombine lhs, ResultCombine rhs)
{
    return static_cast<ResultCombine>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

inline uint8_t operator&(ResultCombine lhs, ResultCombine rhs)
{
    return static_cast<uint8_t>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

//! \brief Scan result that has a content link
struct ScanFoundImage
{
    ScanFoundImage(const std::string& url, const std::string& referrer, bool stripoptions = false) :
        URL(url), Referrer(referrer), StripOptionsOnCompare(stripoptions)
    {
    }

    bool operator==(const ScanFoundImage& other) const
    {
        if (Leviathan::StringOperations::BaseHostName(URL) != Leviathan::StringOperations::BaseHostName(other.URL))
        {
            return false;
        }

        return Leviathan::StringOperations::URLPath(URL, StripOptionsOnCompare) ==
            Leviathan::StringOperations::URLPath(other.URL, other.StripOptionsOnCompare);
    }

    //! \brief Merges tags from other
    ResultCombine Merge(const ScanFoundImage& other)
    {
        ResultCombine resultCombine = ResultCombine::NoNewContent;

        for (const auto& otherTag : other.Tags)
        {
            bool found = false;

            for (const auto& existingTag : Tags)
            {
                if (otherTag == existingTag)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                resultCombine = ResultCombine::NewResults | ResultCombine::NewTags;
                Tags.emplace_back(otherTag);
            }
        }

        return resultCombine;
    }

    std::string URL;
    std::string Referrer;
    std::vector<std::string> Tags;

    //! If true when comparing to another the options (everything after '?') is ignored
    bool StripOptionsOnCompare = false;
};

inline ResultCombine CombineResultCombineValues(ResultCombine left, ResultCombine right)
{
    if (left == ResultCombine::NoNewContent && right == ResultCombine::NoNewContent)
        return ResultCombine::NoNewContent;

    return left | right;
}

//! \brief Result data for IWebsiteScanner
struct ScanResult
{
public:
    //! \brief Used by scanners to add a link with no tags
    ResultCombine AddContentLink(const ScanFoundImage& link)
    {
        for (auto& existingLink : ContentLinks)
        {
            if (existingLink == link)
            {
                return existingLink.Merge(link);
            }
        }

        ContentLinks.push_back(link);
        return ResultCombine::NewResults | ResultCombine::NewContent;
    }

    //! \brief Used by scanners when more pages for a gallery are found
    ResultCombine AddSubpage(const std::string& url)
    {
        for (auto& existingLink : PageLinks)
        {
            if (existingLink == url)
                return ResultCombine::NoNewContent;
        }

        PageLinks.push_back(url);
        return ResultCombine::NewResults | ResultCombine::NewPages;
    }

    //! \brief Used by scanners to add tags to currently scanned thing
    ResultCombine AddTagStr(const std::string& tag)
    {
        for (auto& existingTag : PageTags)
        {
            if (existingTag == tag)
                return ResultCombine::NoNewContent;
        }

        PageTags.push_back(tag);
        return ResultCombine::NewResults | ResultCombine::NewTags;
    }

    void PrintInfo() const
    {
        LOG_INFO("ScanResult: has " + Convert::ToString(ContentLinks.size()) + " found images and " +
            Convert::ToString(PageLinks.size()) + " page links and " + Convert::ToString(PageTags.size()) +
            " page tags");
    }

    ResultCombine Combine(const ScanResult& other)
    {
        ResultCombine result = ResultCombine::NoNewContent;

        for (const auto& inOther : other.ContentLinks)
            result = CombineResultCombineValues(result, AddContentLink(inOther));

        for (const auto& inOther : other.PageLinks)
            result = CombineResultCombineValues(result, AddSubpage(inOther));

        for (const auto& inOther : other.PageTags)
            result = CombineResultCombineValues(result, AddTagStr(inOther));

        if (!other.PageTitle.empty())
            PageTitle += "; " + other.PageTitle;

        return result;
    }

    std::vector<ScanFoundImage> ContentLinks;
    std::vector<std::string> PageLinks;
    std::vector<std::string> PageTags;

    //! Title of the scanned page
    //! \note Scan plugins should remove unneeded parts from this. For example if the
    //! title has the site name that should be removed
    std::string PageTitle;
};

//! \brief Data for IWebsiteScanner::ScanSite
struct SiteToScan
{
    const std::string& Body;
    const std::string& URL;
    const std::string& ContentType;

    bool InitialPage;
};

//! \brief Implementation of a website scanner
class IWebsiteScanner
{
public:
    //! \brief Returns a user readable name string, may also contain version
    virtual const char* GetName() = 0;

    //! \brief Returns true if this plugin can handle url
    virtual bool CanHandleURL(const std::string& url) = 0;

    //! \brief Returns true if this plugin uses URL rewriting
    virtual bool UsesURLRewrite() = 0;

    //! \brief Returns URL after rewriting.
    //!
    //! Only valid if UsesURLRewrite returns true
    virtual std::string RewriteURL(const std::string& url) = 0;

    //! \brief Scans a webpage
    //! \param contenttype Content type sent by the server. Probably equals "text/html"
    //! but it may have junk after it so maybe use std::string::find("text/html") ... for
    //! checking
    virtual ScanResult ScanSite(const SiteToScan& params) = 0;

    //! \brief Returns true if this scanner considers the link to be a page that contains
    //! only a single image and is not actually a gallery
    virtual bool IsUrlNotGallery(const std::string& url) = 0;

    //! \brief If this returns true, then the page is scanned again if
    //! no images have been found
    //! \note This is rescanned up to a maximum of 5 times
    virtual bool ScanAgainIfNoImages(const std::string& url) = 0;
};

//! \brief Description of a plugin
//!
//! When loading plugins this is the first thing that is loaded from the plugin and
//! based on the definitions the plugin is added to the right places to be used
//! later
class IPluginDescription
{
public:
    //! \returns A unique string from uuid for this plugin
    virtual const char* GetUUID() = 0;

    //! \returns The (user readable) name of the plugin
    virtual const char* GetPluginName() = 0;

    //! \returns The constant DUALVIEW_VERSION
    virtual const char* GetDualViewVersionStr() = 0;

    //! \returns A list of Website scanners
    virtual std::vector<std::shared_ptr<IWebsiteScanner>> GetSupportedSites() = 0;

    //! \brief For checking that everything is fine
    virtual std::string GetTheAnswer() = 0;
};

// These function pointers must match the functions that the plugins expose
// Also they must be called CreatePluginDesc DestroyPluginDesc and defined
// with c linkage
using CreateDescriptionFuncPtr = IPluginDescription* (*)();
using DestroyDescriptionFuncPtr = void (*)(IPluginDescription*);

} // namespace DV
