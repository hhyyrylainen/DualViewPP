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

    //! \brief Overrides referrer in original URL
    ProcessableURL(const ProcessableURL& originalUrl, std::string newReferrer) :
        URL(originalUrl.URL), Canonical(originalUrl.Canonical), Referrer(std::move(newReferrer)),
        Cookies(originalUrl.Cookies){};

    ProcessableURL(
        const std::string_view& url, const std::string_view& canonicalUrl, const std::string_view& referrer) :
        URL(url),
        Canonical(canonicalUrl), Referrer(referrer){};

    //! \brief Constructor when canonical URL is not known, noCanonicalUrl has to be true
    ProcessableURL(std::string url, bool noCanonicalUrl) : URL(std::move(url))
    {
        if (!noCanonicalUrl)
            throw std::runtime_error("no canonical parameter has to be always true");
    }

    ProcessableURL(std::string url, bool noCanonicalUrl, std::string referrer) :
        URL(std::move(url)), Referrer(std::move(referrer))
    {
        if (!noCanonicalUrl)
            throw std::runtime_error("no canonical parameter has to be always true");
    }

    ProcessableURL(const ProcessableURL& other) = default;

    ProcessableURL(ProcessableURL&& other) noexcept :
        URL(std::move(other.URL)), Canonical(std::move(other.Canonical)), Referrer(std::move(other.Referrer)),
        Cookies(std::move(other.Cookies))
    {
    }

    ProcessableURL& operator=(const ProcessableURL& other) = default;

    ProcessableURL& operator=(ProcessableURL&& other) noexcept
    {
        URL = std::move(other.URL);
        Canonical = std::move(other.Canonical);
        Referrer = std::move(other.Referrer);
        Cookies = std::move(other.Cookies);
        return *this;
    }

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

    [[nodiscard]] bool HasCanonicalURL() const noexcept
    {
        return !Canonical.empty() && Canonical != URL;
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

    [[nodiscard]] bool HasCookies() const noexcept
    {
        return !Cookies.empty();
    }

    [[nodiscard]] const std::string& GetCookies() const noexcept
    {
        return Cookies;
    }

    void SetCookies(std::string cookies) noexcept
    {
        Cookies = std::move(cookies);
    }

    void SetCookies(const std::string_view& cookies) noexcept
    {
        Cookies = cookies;
    }

private:
    std::string URL;
    std::string Canonical;
    std::string Referrer;

    std::string Cookies;
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
