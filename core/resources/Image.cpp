// ------------------------------------ //
#include "Image.h"

#include "DualView.h"
#include "CacheManager.h"
#include "Exceptions.h"
#include "FileSystem.h"
#include "Common/StringOperations.h"
#include "Common.h"

#include <boost/filesystem.hpp>
#include <cryptopp/sha.h>

#include "third_party/base64.h"

using namespace DV;
// ------------------------------------ //

Image::Image(const std::string &file) : ResourcePath(file), ImportLocation(file)
{
    if(!boost::filesystem::exists(file)){

        throw Leviathan::InvalidArgument("Image: file doesn't exist");
    }
    
    ResourceName = boost::filesystem::path(ResourcePath).filename().string();
    Extension = boost::filesystem::path(ResourcePath).extension().string();


}

void Image::Init(){

    if(!IsReadyToAdd){
        // Register hash calculation //
        _QueueHashCalculation();
    }
}
// ------------------------------------ //
std::shared_ptr<LoadedImage> Image::GetImage() const{

    return DualView::Get().GetCacheManager().LoadFullImage(ResourcePath);
}
    
std::shared_ptr<LoadedImage> Image::GetThumbnail() const{

    if(!IsReadyToAdd)
        return nullptr;

    return DualView::Get().GetCacheManager().LoadThumbImage(ResourcePath, Hash);
}

std::string Image::GetHash() const{

    if(!IsReadyToAdd)
        throw Leviathan::InvalidState("Hash hasn't been calculated");

    return Hash;
}
// ------------------------------------ //
std::string Image::CalculateFileHash() const{

    // Load file bytes //
    std::string contents;
    if(!Leviathan::FileSystem::ReadFileEntirely(ResourcePath, contents)){

        LEVIATHAN_ASSERT(0, "Failed to read file for hash calculation");
    }

    // Calculate sha256 hash //
    byte digest[CryptoPP::SHA256::DIGESTSIZE];

    CryptoPP::SHA256().CalculateDigest(digest, reinterpret_cast<const byte*>(
            contents.data()), contents.length());

    static_assert(sizeof(digest) == CryptoPP::SHA256::DIGESTSIZE, "sizeof funkyness");

    // Encode it //
    std::string hash = base64_encode(digest, sizeof(digest));

    // Make it path safe //
    hash = Leviathan::StringOperations::ReplaceSingleCharacter<std::string>(hash, "/", '_');

    return hash;
}

void Image::_DoHashCalculation(){

    Hash = CalculateFileHash();

    LEVIATHAN_ASSERT(!Hash.empty(), "Image created an empty hash");

    IsReadyToAdd = true;
}
// ------------------------------------ //
void Image::_QueueHashCalculation(){

    DualView::Get().QueueImageHashCalculate(shared_from_this());
}
// ------------------------------------ //
void Image::BecomeDuplicateOf(const Image &other){

    DEBUG_BREAK;
}

