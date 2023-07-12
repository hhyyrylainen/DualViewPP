// ------------------------------------ //
#include "ScanResult.h"

#include "Common/StringOperations.h"

// ------------------------------------ //
namespace DV
{
// ------------------------------------ //
// ScanFoundImage
ScanFoundImage::ScanFoundImage(ProcessableURL url) : URL(std::move(url))
{
}

bool ScanFoundImage::operator==(const ScanFoundImage& other) const
{
    return URL == other.URL;
}

// ------------------------------------ //
// ScanResult
ResultCombine ScanResult::AddContentLink(const ScanFoundImage& link)
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

ResultCombine ScanResult::AddSubpage(const ProcessableURL& url)
{
    for (auto& existingLink : PageLinks)
    {
        if (existingLink == url)
            return ResultCombine::NoNewContent;
    }

    PageLinks.push_back(url);
    return ResultCombine::NewResults | ResultCombine::NewPages;
}

ResultCombine ScanResult::AddTagStr(const std::string& tag)
{
    for (auto& existingTag : PageTags)
    {
        if (existingTag == tag)
            return ResultCombine::NoNewContent;
    }

    PageTags.push_back(tag);
    return ResultCombine::NewResults | ResultCombine::NewTags;
}

// ------------------------------------ //
ResultCombine ScanResult::Combine(const ScanResult& other)
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

// ------------------------------------ //
void ScanResult::PrintInfo() const
{
    LOG_INFO("ScanResult: has " + Convert::ToString(ContentLinks.size()) + " found images and " +
        Convert::ToString(PageLinks.size()) + " page links and " + Convert::ToString(PageTags.size()) + " page tags");

    if (ContentLinks.size() == 1)
    {
        const auto& url = ContentLinks.begin()->URL;
        LOG_INFO("ScanResult: found content: " + url.GetURL());

        if (url.HasCanonicalURL())
        {
            LOG_INFO("ScanResult: canonical URL is: " + url.GetCanonicalURL());
        }
    }
}
} // namespace DV
