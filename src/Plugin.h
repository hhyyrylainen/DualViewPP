#pragma once

#include <string>

#include "Common.h"
#include "ProcessableURL.h"
#include "ScanResult.h"

namespace DV
{
//! \brief Data for IWebsiteScanner::ScanSite
struct SiteToScan
{
    //! The entire downloaded body. This is a string reference as the data is buffered there anyway and a scanner
    //! library that is used by the example plugin must be given a string
    const std::string& Body;
    const ProcessableURL& URL;
    const std::string_view ContentType;

    bool InitialPage;
};

//! \brief Implementation of a website scanner
class IWebsiteScanner
{
public:
    //! \brief Returns a user readable name string, may also contain version
    virtual const char* GetName() = 0;

    //! \brief Returns true if this plugin can handle url
    //!
    //! \param url The url to check, this doesn't have rewriting or canonization performed on it
    //! \return True if this plugin can handle the URL. The first plugin to say it can handle an URL is always picked
    virtual bool CanHandleURL(const std::string& url) = 0;

    //! \brief Returns true if this plugin uses URL rewriting
    virtual bool UsesURLRewrite() = 0;

    //! \brief Returns URL after rewriting.
    //!
    //! Only valid if UsesURLRewrite returns true
    virtual std::string RewriteURL(const std::string& url) = 0;

    //! \brief Returns true if this supports ConvertToCanonicalURL
    virtual bool HasCanonicalURLFeature() const = 0;

    //! \brief Converts a URL to a form that doesn't have any unnecessary parameters
    //!
    //! Canonical URLs are used to prevent the scanning or detecting of a duplicate content image / page
    //! \param url The url to convert
    //! \return The converted URL or the input URL if there is nothing to modify
    virtual std::string ConvertToCanonicalURL(const std::string& url) = 0;

    //! \brief Scans a webpage
    //! \param params Contains the content type sent by the server. Probably equals "text/html"
    //! but it may have junk after it so maybe use std::string::find("text/html") ... for
    //! checking
    virtual ScanResult ScanSite(const SiteToScan& params) = 0;

    //! \brief Returns true if this scanner considers the link to be a page that contains
    //! only a single image and is not actually a gallery
    virtual bool IsUrlNotGallery(const ProcessableURL& url) = 0;

    //! \brief If this returns true, then the page is scanned again if
    //! no images have been found
    //! \note This is rescanned up to a maximum of 5 times
    virtual bool ScanAgainIfNoImages(const ProcessableURL& url) = 0;
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
