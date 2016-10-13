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

            LOG_FATAL("Failed to initialize / download timezone database");
            throw;
        }
        

        IsInitialized = true;
    }

    static auto parse8601(const std::string &str)
    {
        TimeZoneDatabaseSetup();

        //LOG_INFO("Parsing time: " + str);
        
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
    }

    //! \todo Check does this need to call TimeZoneDatabaseSetup
    static auto parse8601UTC(const std::string &str)
    {
        //TimeZoneDatabaseSetup();
        
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

        return tp;
    }

    template<class TZonedTime>
        static auto format8601(const TZonedTime &time)
    {
        return date::format("%FT%T%Ez", time);
    }

    //! \brief Formats current zoned time as a string
    static auto FormatCurrentTimeAs8601(){

        return format8601(date::make_zoned(date::current_zone(),
                std::chrono::time_point_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now())));
    }
    
private:
    
    static std::atomic<bool> IsInitialized;
    static std::mutex InitializeMutex;
};



}

#undef INSTALL
