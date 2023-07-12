// ------------------------------------ //
#include "ProcessableURL.h"

// ------------------------------------ //
namespace DV
{
ProcessableURL::ProcessableURL(std::string url, bool noCanonicalUrl) : URL(std::move(url))
{
    if (!noCanonicalUrl)
        throw std::runtime_error("no canonical parameter has to be always true");
}

ProcessableURL::ProcessableURL(std::string url, bool noCanonicalUrl, std::string referrer) :
    URL(std::move(url)), Referrer(std::move(referrer))
{
    if (!noCanonicalUrl)
        throw std::runtime_error("no canonical parameter has to be always true");
}

// ------------------------------------ //
ProcessableURL& ProcessableURL::operator=(ProcessableURL&& other) noexcept
{
    URL = std::move(other.URL);
    Canonical = std::move(other.Canonical);
    Referrer = std::move(other.Referrer);
    Cookies = std::move(other.Cookies);
    return *this;
}

} // namespace DV
