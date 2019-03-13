#pragma once

#include "Common.h"
#include "IsAlive.h"

#include <functional>
#include <memory>
#include <vector>

namespace DV {

class Image;

constexpr auto SIGNATURE_CALCULATOR_READ_MORE_THRESSHOLD = 5;
constexpr auto SIGNATURE_CALCULATOR_READ_BATCH = 50;
constexpr auto SIGNATURE_CALCULATOR_GROUP_IMAGE_SAVE = 100;

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
    void Pause(bool wait = false);

    bool IsDone() const;


    //! \brief A callback for getting status updates.
    //! \note This can be called from a background thread
    void SetStatusListener(std::function<void(int processed, int total, bool done)> callback);

    //! \brief Calculates the signatures for the given image
    //! \returns False on error
    //! \note This shares the static puzzle context and uses a lock to protect access to it
    bool CalculateImageSignature(Image& image);

private:
    void _RunCalculationThread();

    void _ReportStatus() const;

    void _SaveQueueHelper(
        std::vector<std::shared_ptr<Image>>& savequeue, bool runinbackground);

private:
    std::unique_ptr<Private> pimpl;
};

} // namespace DV
