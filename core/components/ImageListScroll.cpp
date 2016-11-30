// ------------------------------------ //
#include "ImageListScroll.h"

using namespace DV;
// ------------------------------------ //
bool ImageListScroll::HasCount() const{

    return false;
}

size_t ImageListScroll::GetCount() const{

    return 0;
}
// ------------------------------------ //
bool ImageListScroll::SupportsRandomAccess() const{

    return false;
}

std::shared_ptr<Image> ImageListScroll::GetImageAt(size_t index) const{

    return nullptr;
}

size_t ImageListScroll::GetImageIndex(Image& image) const{

    return 0;
}
// ------------------------------------ //
std::string ImageListScroll::GetDescriptionStr() const{

    return "";
}

// ------------------------------------ //
// ImageListScrollVector
ImageListScrollVector::ImageListScrollVector(const std::vector<std::shared_ptr<Image>>
    &images) :
    Images(images)
{

}
// ------------------------------------ //
std::shared_ptr<Image> ImageListScrollVector::GetNextImage(std::shared_ptr<Image> current,
    bool wrap /*= true*/)
{
    if(!current)
        return nullptr;
    
    const auto index = GetImageIndex(*current);

    if(index >= Images.size())
        return nullptr;

    if(index + 1 < Images.size()){

        // Next is at index + 1
        return Images[index + 1];
        
    } else {

        // current is the last image //
        if(!wrap)
            return nullptr;

        return Images[0];
    }    
}

std::shared_ptr<Image> ImageListScrollVector::GetPreviousImage(std::shared_ptr<Image> current,
    bool wrap /*= true*/)
{
    if(!current)
        return nullptr;
    
    const auto index = GetImageIndex(*current);

    if(index >= Images.size())
        return nullptr;

    if(index >= 1){

        // Previous is at index - 1
        return Images[index - 1];
        
    } else {

        // current is the first image //
        if(!wrap)
            return nullptr;

        return Images[Images.size() - 1];
    }
}
// ------------------------------------ //
bool ImageListScrollVector::HasCount() const{

    return true;
}

size_t ImageListScrollVector::GetCount() const{
    
    return Images.size();
}

bool ImageListScrollVector::SupportsRandomAccess() const{

    return true;
}

std::shared_ptr<Image> ImageListScrollVector::GetImageAt(size_t index) const{

    if(index >= Images.size())
        return nullptr;

    return Images[index];
}

size_t ImageListScrollVector::GetImageIndex(Image& image) const{

    for(size_t i = 0; i < Images.size(); ++i){

        if(Images[i].get() == &image)
            return i;
    }

    return -1;
}
// ------------------------------------ //
std::string ImageListScrollVector::GetDescriptionStr() const{
    return "list";
}
