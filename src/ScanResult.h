#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Common/StringOperations.h"

#include "ProcessableURL.h"

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

inline ResultCombine CombineResultCombineValues(ResultCombine left, ResultCombine right)
{
    if (left == ResultCombine::NoNewContent && right == ResultCombine::NoNewContent)
        return ResultCombine::NoNewContent;

    return left | right;
}

//! \brief Scan result that has a content link
struct ScanFoundImage
{
public:
    explicit ScanFoundImage(ProcessableURL url) : URL(std::move(url))
    {
    }

    bool operator==(const ScanFoundImage& other) const
    {
        return URL == other.URL;
    }

    //! \brief Merges tags from other
    ResultCombine Merge(const ScanFoundImage& other)
    {
        auto resultCombine = ResultCombine::NoNewContent;

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

    ProcessableURL URL;
    std::vector<std::string> Tags;
};

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
    ResultCombine AddSubpage(const ProcessableURL& url)
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

    void PrintInfo() const
    {
        LOG_INFO("ScanResult: has " + Convert::ToString(ContentLinks.size()) + " found images and " +
            Convert::ToString(PageLinks.size()) + " page links and " + Convert::ToString(PageTags.size()) +
            " page tags");

        if (ContentLinks.size() == 1)
        {
            const auto& url = ContentLinks.begin()->URL;
            LOG_INFO("ScanResult: found single content: " + url.GetURL());

            if (url.HasCanonicalURL())
            {
                LOG_INFO("ScanResult: canonical URL is: " + url.GetCanonicalURL());
            }
        }
    }

    std::vector<ScanFoundImage> ContentLinks;
    std::vector<ProcessableURL> PageLinks;
    std::vector<std::string> PageTags;

    //! Title of the scanned page
    //! \note Scan plugins should remove unneeded parts from this. For example if the
    //! title has the site name that should be removed
    std::string PageTitle;
};

} // namespace DV
