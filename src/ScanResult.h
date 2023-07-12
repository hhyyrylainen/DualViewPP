#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

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
    ScanFoundImage(std::string url, std::string referrer, bool stripOptions = false);

    bool operator==(const ScanFoundImage& other) const;

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

//! \brief Result data for IWebsiteScanner
struct ScanResult
{
public:
    //! \brief Used by scanners to add a link with no tags
    ResultCombine AddContentLink(const ScanFoundImage& link);

    //! \brief Used by scanners when more pages for a gallery are found
    ResultCombine AddSubpage(const std::string& url);

    //! \brief Used by scanners to add tags to currently scanned thing
    ResultCombine AddTagStr(const std::string& tag);

    ResultCombine Combine(const ScanResult& other);

    void PrintInfo() const;

    std::vector<ScanFoundImage> ContentLinks;
    std::vector<std::string> PageLinks;
    std::vector<std::string> PageTags;

    //! Title of the scanned page
    //! \note Scan plugins should remove unneeded parts from this. For example if the
    //! title has the site name that should be removed
    std::string PageTitle;
};

} // namespace DV
