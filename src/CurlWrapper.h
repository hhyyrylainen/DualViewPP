#pragma once

#include <mutex>

#include <curl/curl.h>


namespace DV{

//! \brief A simple class for holding a curl instance
class CurlWrapper{
public:

    //! \brief Creates a curl instance
    CurlWrapper();

    //! \brief Releases a curl instance
    ~CurlWrapper();

    CURL* Get(){

        return WrappedObject;
    }

protected:

    CURL* WrappedObject = nullptr;

    // Global libcurl init code
    static void CheckCurlInit();
    static std::mutex CurlInitMutex;
    static bool CurlInitialized;
};
}
