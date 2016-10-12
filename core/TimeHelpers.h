#pragma once

#include <atomic>

#include "Common.h"

#include "CurlWrapper.h"
#include "third_party/date/tz.h"

//! \file Helper functions for saving / loading times from the database

namespace DV{

//! \brief Time parsing functions
class TimeHelpers{
public:
    
    static inline void TimeZoneDatabaseSetup(){

        if(IsInitialized)
            return;

        std::lock_guard<std::mutex> lock(InitializeMutex);

        if(IsInitialized)
            return;
        
        // Make sure curl is initialized //
        CurlWrapper wrapper;

        try{
            date::get_tzdb();

        } catch(...){

            LOG_ERROR("Failed to initialize / download timezone database");
            throw;
        }
        

        IsInitialized = true;
    }

    static auto parse8601(const std::string &str)
    {
        TimeZoneDatabaseSetup();
        
        std::istringstream in(str);

        date::sys_time<std::chrono::milliseconds> tp;
        date::parse(in, "%FT%TZ", tp);
    
        if (in.fail())
        {
            in.clear();
            in.exceptions(std::ios::failbit);
            in.str(str);
            date::parse(in, "%FT%T%Ez", tp);
        }

        return date::make_zoned(date::current_zone(), tp);
        //return date::make_zoned(tp);
    }
    
private:
    
    static std::atomic<bool> IsInitialized;
    static std::mutex InitializeMutex;
};



}

#undef INSTALL
