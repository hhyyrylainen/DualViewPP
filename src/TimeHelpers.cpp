// ------------------------------------ //
#include "TimeHelpers.h"

using namespace DV;
// ------------------------------------ //

std::atomic<bool> TimeHelpers::IsInitialized = { false };
std::mutex TimeHelpers::InitializeMutex;
std::shared_ptr<date::zoned_time<std::chrono::milliseconds>> TimeHelpers::StartTime;
