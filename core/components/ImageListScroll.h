#pragma once

#include <string>
#include <memory>
#include <vector>

namespace DV{

class Image;

//! \brief Provides a way for SuperViewer to move inside a list of images
class ImageListScroll{
public:

    // Core interface //
    //! \brief Returns the next image
    //! \param wrap If true and current is the last image will return the first image
    virtual std::shared_ptr<Image> GetNextImage(std::shared_ptr<Image> current,
        bool wrap = true) = 0;

    //! \brief Returns the previous image
    //! \param wrap If true and current is the last first will return the last image
    virtual std::shared_ptr<Image> GetPreviousImage(std::shared_ptr<Image> current,
        bool wrap = true) = 0;

    // Optional interface //
    
    //! \brief Returns true if GetCount is valid
    virtual bool HasCount() const;

    //! \brief Returns the number of images
    virtual size_t GetCount() const;

    //! \brief Returns true if random access is supported
    //! \see GetImageAt
    virtual bool SupportsRandomAccess() const;

    //! \brief Returns image at index or null if out of range
    virtual std::shared_ptr<Image> GetImageAt(size_t index) const;

    //! \brief Returns index of image. Only valid if SupportsRandomAccess is true
    virtual size_t GetImageIndex(Image& image) const;

    //! \brief Returns description or empty string if not supported
    virtual std::string GetDescriptionStr() const;
};

//! \brief ImageListScroll with a vector holding the images
class ImageListScrollVector : public ImageListScroll{
public:

    ImageListScrollVector(const std::vector<std::shared_ptr<Image>> &images);

    // ImageListScroll implementation //
    std::shared_ptr<Image> GetNextImage(std::shared_ptr<Image> current, bool wrap = true)
        override;

    std::shared_ptr<Image> GetPreviousImage(std::shared_ptr<Image> current, bool wrap = true)
        override;

    bool HasCount() const override;

    size_t GetCount() const override;

    bool SupportsRandomAccess() const override;

    std::shared_ptr<Image> GetImageAt(size_t index) const override;

    size_t GetImageIndex(Image& image) const override;

    std::string GetDescriptionStr() const override;

protected:

    std::vector<std::shared_ptr<Image>> Images;
};




}

