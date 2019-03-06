//! \file Contains plugin implementation for an imgur downloader
// ------------------------------------ //
#include "src/Plugin.h"

#include "Common.h"

#include "Common/StringOperations.h"

#include "GQ/src/Document.hpp"

#include "json.hpp"

#include <regex>

using namespace DV;
using json = nlohmann::json;
// ------------------------------------ //

static const auto IUContentLink =
    std::regex(".*i.imgur.com\\/.+", std::regex::ECMAScript | std::regex::icase);


//! This seems to be an alternative format to ImgurIDCapture
static const auto ImgurIDCaptureGallery =
    std::regex(R"(.*imgur.com\/gallery\/(\w+).*)", std::regex::ECMAScript | std::regex::icase);

static const auto ImgurIDCapture =
    std::regex(R"(.*imgur.com\/a\/(\w+).*)", std::regex::ECMAScript | std::regex::icase);

class ImgurScanner final : public IWebsiteScanner {


    const char* GetName() override
    {
        return "Imgur Downloader";
    }

    bool CanHandleURL(const std::string& url) override
    {
        if(url.find("imgur.com") != std::string::npos)
            return true;

        return false;
    }

    //! For parsing html if for some reason that is returned
    void ScanHtml(const std::string& body, const std::string& url, ScanResult& result)
    {
        LOG_INFO("ImgurScanner: scanning page: " + url);

        auto testDocument = gq::Document::Create();
        testDocument->Parse(body);

        try {

            testDocument->Each(".post-images", [&](const gq::Node* node) {
                // Directly images //
                node->Each("img", [&](const gq::Node* image) {
                    if(!image->HasAttribute(boost::string_ref("src")))
                        return;

                    auto link = image->GetAttributeValue(boost::string_ref("src"));

                    if(std::regex_match(std::begin(link), std::end(link), IUContentLink)) {
                        LOG_INFO("Found type 2 (direct image)");
                        // Combine the url to make it absolute //
                        result.AddContentLink(ScanFoundImage(
                            Leviathan::StringOperations::CombineURL(url, link.to_string()),
                            url));
                    }
                });

                node->Each(".post-image-container", [&](const gq::Node* contentLink) {
                    const auto linkid =
                        contentLink->GetAttributeValue(boost::string_ref("id"));

                    // For gifs this is required //
                    // TODO: webms
                    auto videos = contentLink->Find(".video-container");

                    for(size_t i = 0; i < videos.GetNodeCount(); ++i) {

                        LOG_INFO("Found gif");

                        // Needs to download as gif
                        result.AddContentLink(
                            ScanFoundImage(Leviathan::StringOperations::URLProtocol(url) +
                                               "://i.imgur.com/" + linkid.to_string() + ".gif",
                                url));
                    }

                    // If this doesn't have an img tag (or a video container)
                    // then this is unloaded
                    // But forcing ?grid should put all the images on one page
                    // result.AddSubpage(Leviathan::StringOperations::CombineURL(url,
                    //        "/" + linkid.to_string()));
                });
            });
        } catch(std::runtime_error& e) {
            // Errors from the css selection
            LOG_ERROR("GQ selector error: " + std::string(e.what()));
        }
    }

    //! URL rewrite is used to actually retrieve a json object defining the images
    bool UsesURLRewrite() override
    {
        return true;
    }

    //! Helper for RewriteURL
    std::string GetAjaxURL(const std::string& url, const std::ssub_match& match) const
    {
        return Leviathan::StringOperations::URLProtocol(url) +
               "://imgur.com/ajaxalbums/getimages/" + match.str() + "/hit.json";
    }

    std::string RewriteURL(const std::string& url) override
    {
        std::smatch matches;

        if(std::regex_match(url, matches, ImgurIDCapture)) {

            // The second match is the one we want
            if(matches.size() == 2) {

                return GetAjaxURL(url, matches[1]);
            }

        } else if(std::regex_match(url, matches, ImgurIDCaptureGallery)) {

            // The second match is the one we want
            if(matches.size() == 2) {

                return GetAjaxURL(url, matches[1]);
            }
        }

        // Wasn't a valid imgur URL
        LOG_WARNING("Imgur rewrite failed for url: " + url);
        return url;
    }

    ScanResult ScanSite(const SiteToScan& params) override
    {
        ScanResult result;

        if(params.ContentType == "application/json") {

            LOG_INFO("Parsing imgur json API");
            LOG_WRITE("TODO: get page title somehow");

            try {

                auto j = json::parse(params.Body);

                bool valid = true;

                // data list has all the images
                if(j.find("data") != j.end()) {

                    // has that list //
                    int32_t count = j["data"]["count"];

                    LOG_INFO(
                        "Imgur downloader: expecting " + Convert::ToString(count) + " images");

                    for(const auto& image : j["data"]["images"]) {

                        std::string extension = image["ext"];

                        const std::string name = image["hash"];

                        const std::string description = image["title"];

                        // Force gifs for webms and other videos //
                        if(extension == ".webm" || extension == ".mp4")
                            extension = ".gif";

                        // Create link and add //
                        result.AddContentLink(ScanFoundImage(
                            Leviathan::StringOperations::URLProtocol(params.URL) +
                                "://i.imgur.com/" + name + extension,
                            params.URL));
                    }
                }

                if(!valid) {

                    LOG_ERROR("Imgur json format has changed! this was not processed "
                              "correctly:");
                    LOG_WRITE(j.dump(4));
                }

            } catch(const std::exception& e) {

                LOG_ERROR("Imgur downloader: json parsing error: " + std::string(e.what()));
                return result;
            }

        } else if(params.ContentType.find("text/html") != std::string::npos) {

            ScanHtml(params.Body, params.URL, result);

        } else {

            LOG_ERROR("Imgur downloader got unknown content type: " + params.ContentType);
        }

        return result;
    }

    bool IsUrlNotGallery(const std::string& url) override
    {
        return false;
    }

    bool ScanAgainIfNoImages(const std::string& url) override
    {
        return false;
    }
};


class ImgurPluginDescription final : public IPluginDescription {

    const char* GetUUID() override
    {
        return "b1ed014c-a90e-11e6-92f7-305a3a06584e";
    }

    //! \returns The (user readable) name of the plugin
    const char* GetPluginName() override
    {
        return "Imgur Download Plugin";
    }

    const char* GetDualViewVersionStr() override
    {
        return DUALVIEW_VERSION;
    }

    std::vector<std::shared_ptr<IWebsiteScanner>> GetSupportedSites() override
    {
        return {
            std::shared_ptr<IWebsiteScanner>(new ImgurScanner(), [=](IWebsiteScanner* obj) {
                auto* casted = static_cast<ImgurScanner*>(obj);
                delete casted;
            })};
    }

    std::string GetTheAnswer() override
    {
        return std::string("42");
    }
};

extern "C" {

IPluginDescription* CreatePluginDesc()
{
    return new ImgurPluginDescription;
}

void DestroyPluginDesc(IPluginDescription* desc)
{
    if(!desc)
        return;

    auto* casted = static_cast<ImgurPluginDescription*>(desc);
    delete casted;
}
}
