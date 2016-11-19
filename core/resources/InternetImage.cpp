// ------------------------------------ //
#include "InternetImage.h"


#include "core/DualView.h"
#include "core/Settings.h"
#include "core/DownloadManager.h"

#include "leviathan/Common/StringOperations.h"
#include "Exceptions.h"

#include <regex>
#include <boost/filesystem.hpp>

using namespace DV;
// ------------------------------------ //
InternetImage::InternetImage(const ScanFoundImage &link) :
    DLURL(link.URL), Referrer(link.Referrer)
{
    // Extract filename from the url
    ResourceName = DownloadManager::ExtractFileName(DLURL);

    if(ResourceName.empty())
        throw Leviathan::InvalidArgument("link doesn't contain filename");

    Extension = Leviathan::StringOperations::GetExtension(ResourceName);

    ResourcePath = (boost::filesystem::path(DualView::Get().GetSettings().GetStagingFolder()) /
        GetLocalFilename()).string();

    ImportLocation = DLURL;
}

void InternetImage::Init(){

    // TODO: parse tags
}
// ------------------------------------ //
std::string InternetImage::GetLocalFilename() const{

    const auto hash = DualView::CalculateBase64EncodedHash(DLURL);

    return hash + "." + Extension;
}
