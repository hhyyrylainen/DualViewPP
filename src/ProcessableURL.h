#pragma once

#include <functional>

namespace DV
{
//! \brief URL usable by the scanner and downloader. This exists to keep a canonical representation of an URL for
//! duplicate checking and the real URL for any outgoing network requests.
class ProcessableURL
{
public:
    ProcessableURL(std::string url, std::string canonicalUrl) :
        URL(std::move(url)), Canonical(std::move(canonicalUrl)){};

    ProcessableURL(std::string url, std::string canonicalUrl, std::string referrer) :
        URL(std::move(url)), Canonical(std::move(canonicalUrl)), Referrer(std::move(referrer)){};

    ProcessableURL(const std::string_view& url, const std::string_view& canonicalUrl) :
        URL(url), Canonical(canonicalUrl){};

    ProcessableURL(
        const std::string_view& url, const std::string_view& canonicalUrl, const std::string_view& referrer) :
        URL(url),
        Canonical(canonicalUrl), Referrer(referrer){};

    //! \brief Constructor when canonical URL is not known, noCanonicalUrl has to be true
    ProcessableURL(std::string url, bool noCanonicalUrl);

    ProcessableURL(std::string url, bool noCanonicalUrl, std::string referrer);

    ProcessableURL(const ProcessableURL& other) = default;

    ProcessableURL(ProcessableURL&& other) noexcept : URL(std::move(other.URL)), Canonical(std::move(other.Canonical))
    {
    }

    ProcessableURL& operator=(const ProcessableURL& other) = default;

    ProcessableURL& operator=(ProcessableURL&& other) noexcept;

    inline bool operator==(const ProcessableURL& other) const noexcept
    {
        return GetCanonicalURL() == other.GetCanonicalURL();
    }

    inline bool operator!=(const ProcessableURL& other) const noexcept
    {
        return GetCanonicalURL() != other.GetCanonicalURL();
    }

    bool operator<(const ProcessableURL& other) const noexcept
    {
        return GetCanonicalURL() < other.GetCanonicalURL();
    }

    bool operator>(const ProcessableURL& other) const noexcept
    {
        return GetCanonicalURL() > other.GetCanonicalURL();
    }

    [[nodiscard]] const std::string& GetURL() const noexcept
    {
        return URL;
    }

    [[nodiscard]] const std::string& GetCanonicalURL() const noexcept
    {
        if (Canonical.empty())
            return URL;

        return Canonical;
    }

    [[nodiscard]] bool HasReferrer() const noexcept
    {
        return !Referrer.empty();
    }

    [[nodiscard]] const std::string& GetReferrer() const noexcept
    {
        return Referrer;
    }

    void SetReferrer(std::string referrer) noexcept
    {
        Referrer = std::move(referrer);
    }

    void SetReferrer(const std::string_view& referrer) noexcept
    {
        Referrer = referrer;
    }

private:
    std::string URL;
    std::string Canonical;
    std::string Referrer;
};
} // namespace DV

template<>
struct std::hash<DV::ProcessableURL>
{
    std::size_t operator()(const DV::ProcessableURL& k) const
    {
        return std::hash<std::string>()(k.GetCanonicalURL());
    }
};
