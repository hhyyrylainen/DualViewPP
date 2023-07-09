#pragma once

#include <atomic>
#include <ctime>

#include "date/tz.h"

#include "Common.h"
#include "CurlWrapper.h"

//! \file Helper functions for saving / loading times from the database

namespace DV
{

//! \brief Time parsing functions
class TimeHelpers
{
public:
    static inline void TimeZoneDatabaseSetup()
    {
        if (IsInitialized)
            return;

        std::lock_guard<std::mutex> lock(InitializeMutex);

        if (IsInitialized)
            return;

        // Make sure curl is initialized //
        CurlWrapper wrapper;

        try
        {
            date::get_tzdb();
        }
        catch (const std::exception& e)
        {
            LOG_FATAL("Failed to initialize / download timezone database: " + std::string(e.what()));
            throw;
        }

        StartTime = std::make_shared<date::zoned_time<std::chrono::milliseconds>>(date::make_zoned(date::current_zone(),
            std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now())));

        IsInitialized = true;
    }

    //! Returns a timestamp when TimeZoneDatabaseSetup was called, use for initializing
    //! zoned times
    static auto GetStaleZonedTime()
    {
        TimeZoneDatabaseSetup();

        return *StartTime;
    }

    static auto parse8601(const std::string& str)
    {
        TimeZoneDatabaseSetup();

        // LOG_INFO("Parsing time: " + str);

        std::istringstream in(str);

        date::sys_time<std::chrono::milliseconds> tp;
        in >> date::parse("%FT%TZ", tp);

        if (in.fail())
        {
            in.clear();
            in.exceptions(std::ios::failbit);
            in.str(str);
            in >> date::parse("%FT%T%Ez", tp);
        }

        return date::make_zoned(date::current_zone(), tp);
    }

    //! \todo Check does this need to call TimeZoneDatabaseSetup
    static auto parse8601UTC(const std::string& str)
    {
        // TimeZoneDatabaseSetup();

        std::istringstream in(str);

        date::sys_time<std::chrono::milliseconds> tp;
        in >> date::parse("%FT%TZ", tp);

        if (in.fail())
        {
            in.clear();
            in.exceptions(std::ios::failbit);
            in.str(str);
            in >> date::parse("%FT%T%Ez", tp);
        }

        return tp;
    }

    //! Parses any time thing known to man
    static auto ParseTime(const std::string& str)
    {
        // Cannot be iso-8601 format without a 'T' in the string
        if (str.find_first_of('T') != std::string::npos)
        {
            try
            {
                return parse8601(str);
            }
            catch (const std::exception&)
            {
                // Wasn't according to the iso standard...
            }
        }

        // Try a simple time parsing
        std::istringstream in(str);

        date::sys_time<std::chrono::milliseconds> tp;
        in >> date::parse("%F %T", tp);

        if (in.fail())
        {
            throw std::runtime_error("ParseTime unknown format: " + str);
        }

        return date::make_zoned(date::current_zone(), tp);
    }

    template<class TZonedTime>
    static auto format8601(const TZonedTime& time)
    {
        return date::format("%FT%T%Ez", time);
    }

    //! \brief Formats current zoned time as a string
    static auto FormatCurrentTimeAs8601()
    {
        return format8601(date::make_zoned(date::current_zone(),
            std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now())));
    }

    static auto GetCurrentUnixTimestamp()
    {
        return std::time(nullptr);
    }

    static auto GetCurrentTimestamp()
    {
        return date::make_zoned(date::current_zone(),
            std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()));
    }

private:
    static std::atomic<bool> IsInitialized;
    static std::mutex InitializeMutex;

    static std::shared_ptr<date::zoned_time<std::chrono::milliseconds>> StartTime;
};

} // namespace DV

#undef INSTALL
