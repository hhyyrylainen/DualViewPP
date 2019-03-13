#pragma once

#include "Common.h"
#include "IsAlive.h"

#include <memory>
#include <vector>

namespace DV {

class Image;

constexpr auto SIGNATURE_CALCULATOR_READ_MORE_THRESSHOLD = 5;
constexpr auto SIGNATURE_CALCULATOR_READ_BATCH = 50;

//! \brief Manages calculating signatures for a bunch of images
//! \note This processes items in LIFO order, first image is only processed once finished
class SignatureCalculator : public IsAlive {
    struct Private;

public:
    SignatureCalculator();
    ~SignatureCalculator();

    void AddImages(const std::vector<DBID>& images);
    void AddImages(const std::vector<std::shared_ptr<Image>>& images);

    void Resume();
    void Pause();

    bool IsDone() const;

    //! \brief Calculates the signatures for the given image
    //! \returns False on error
    //! \note This shares the static puzzle context and uses a lock to protect access to it
    bool CalculateImageSignature(Image& image);

private:
    void _RunCalculationThread();

private:
    std::unique_ptr<Private> pimpl;
};

} // namespace DV
