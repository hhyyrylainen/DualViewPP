// ------------------------------------ //
#include "InternetImage.h"


#include "core/DualView.h"
#include "core/Settings.h"
#include "core/DownloadManager.h"

#include "leviathan/Common/StringOperations.h"
#include "leviathan/FileSystem.h"
#include "Exceptions.h"

#include <Magick++.h>

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

    ResourcePath = DownloadManager::GetCachePathForURL(DLURL);

    ImportLocation = DLURL;
}

void InternetImage::Init(){

    // TODO: parse tags
}
// ------------------------------------ //
std::shared_ptr<LoadedImage> InternetImage::GetImage(){

    _CheckFileDownload();

    GUARD_LOCK();

    if(!FullImage){
        
        FullImage = std::make_shared<DownloadLoadedImage>(false);

        if(DLReady){

            // Data is ready to set //
            if(FileDL->GetDownloadedBytes().empty()){

                // Fail //
                FullImage->OnFail(FullImage);
            } else {

                FullImage->OnSuccess(FullImage, FileDL->GetDownloadedBytes());
            }
        }
    }

    return FullImage;
}
    
std::shared_ptr<LoadedImage> InternetImage::GetThumbnail(){

    _CheckFileDownload();

    GUARD_LOCK();

    if(!ThumbImage){
        
        ThumbImage = std::make_shared<DownloadLoadedImage>(true);

        if(DLReady){

            // Data is ready to set //
            if(FileDL->GetDownloadedBytes().empty()){

                // Fail //
                ThumbImage->OnFail(ThumbImage);
            } else {

                ThumbImage->OnSuccess(ThumbImage, FileDL->GetDownloadedBytes());
            }
        }
    }

    return ThumbImage;
}

// ------------------------------------ //
void InternetImage::_CheckFileDownload(){

    GUARD_LOCK();
    
    if(FileDL)
        return;
    
    // Check does the file exist already //
    if(boost::filesystem::exists(ResourcePath)){

        LOG_INFO("InternetImage: hashed url file already exists: " + DLURL + " at path: " +
            ResourcePath);

        FileDL = std::make_shared<LocallyCachedDLJob>(ResourcePath);
        WasAlreadyCached = true;
        
    } else {

        FileDL = std::make_shared<MemoryDLJob>(DLURL, Referrer);
    }

    auto us = std::dynamic_pointer_cast<InternetImage>(shared_from_this());

    LEVIATHAN_ASSERT(us, "InternetImage shared_ptr isn't actually of type InternetImage");

    FileDL->SetFinishCallback([us](DownloadJob &job, bool success){

            GUARD_LOCK_OTHER(*us);

            us->DLReady = true;
            
            us->FileDL->GetDownloadedBytes();
            
            if(!success){

                if(us->ThumbImage)
                    us->ThumbImage->OnFail(us->ThumbImage);

                if(us->FullImage)
                    us->FullImage->OnFail(us->FullImage);
                
            } else {

                if(us->ThumbImage)
                    us->ThumbImage->OnSuccess(us->ThumbImage,
                        us->FileDL->GetDownloadedBytes());

                if(us->FullImage)
                    us->FullImage->OnSuccess(us->FullImage,
                        us->FileDL->GetDownloadedBytes());

                // Save the bytes to disk (if over 10KB) //
                if(!us->WasAlreadyCached && us->FileDL->GetDownloadedBytes().size() > 10000 &&
                    us->AutoSaveCache)
                {
                    LOG_INFO("InternetImage: caching image to: " + us->ResourcePath);
                    us->SaveFileToDisk(guard);
                }
            }
        });

    DualView::Get().GetDownloadManager().QueueDownload(FileDL);
}
// ------------------------------------ //
bool InternetImage::SaveFileToDisk(Lock &guard){

    if(!FileDL || FileDL->GetDownloadedBytes().size() < 1000)
        return false;

    Leviathan::FileSystem::WriteToFile(FileDL->GetDownloadedBytes(), ResourcePath);
    return true;
}
// ------------------------------------ //
// DownloadLoadedImage
DownloadLoadedImage::DownloadLoadedImage(bool thumb) :
    LoadedImage("DownloadLoadedImage"), Thumb(thumb)
{
    
}

void DownloadLoadedImage::OnFail(std::shared_ptr<DownloadLoadedImage> thisobject,
    const std::string &error /*= "HTTP request failed"*/)
{
    OnLoadFail(error);
}

void DownloadLoadedImage::OnSuccess(std::shared_ptr<DownloadLoadedImage> thisobject,
    const std::string &data)
{
    bool resize = Thumb;

    auto datablob = std::make_shared<Magick::Blob>(data.c_str(), data.size());

    DualView::Get().QueueWorkerFunction([=](){

            auto image = std::make_shared<std::vector<Magick::Image>>();

            // Load image //
            readImages(image.get(), *datablob);

            if(image->empty()){

                thisobject->OnFail(thisobject, "Downloaded image is empty or invalid");
                return;
            }

            // Coalesce animated images //
            if(image->size() > 1){

                auto coalesced = std::make_shared<std::vector<Magick::Image>>();
                coalesceImages(coalesced.get(), image->begin(), image->end());

                image = coalesced;
            }

            if(resize && image->size() > 4){

                // Drop all but 4 (+ keep the last frame) frames //
                const auto dropmodulo = image->size() / 4;

                // Keep track of dropped frames //
                size_t losttime = 0;
                size_t actualnumber = 0;
                
                for(size_t i = 0; i < image->size(); )
                {
                    
                    if (actualnumber % dropmodulo != 0 && i + 1 < image->size())
                    {
                        losttime += image->at(i).animationDelay();
                        image->erase(image->begin() + i);

                    } else {
                        
                        // Increase delay of last frame //
                        if(losttime > 0){

                            image->at(i - 1).animationDelay(image->at(i - 1).animationDelay() +
                                losttime);
                            
                            losttime = 0;
                        }
                        
                        ++i;
                    }

                    actualnumber++;
                }

                // Add time to last frame //
                if(losttime > 0)
                    image->at(image->size() - 1).animationDelay(
                        image->at(image->size() - 1).animationDelay() + losttime);
            }

            // Resize all frames //
            if(resize){
                for(auto iter = image->begin(); iter != image->end(); ++iter){

                    iter->resize(CacheManager::CreateResizeSizeForImage(*iter, 128, 0));
                }
            }

            thisobject->OnLoadSuccess(image);            
        });
}

