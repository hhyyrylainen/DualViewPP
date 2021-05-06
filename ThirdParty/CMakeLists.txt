
# For date library
# add_definitions(-DINSTALL=.)

# overridden default values that are required
option(USE_SYSTEM_TZ_DB "Use the operating system's timezone database" ON)
option(ENABLE_DATE_TESTING "Enable unit tests" OFF)
option(BUILD_TZ_LIB "build/install of TZ library" ON)

if(NOT ${BUILD_TZ_LIB})
  message(FATAL_ERROR Build TZ must be on!)
endif()

add_subdirectory(date)

add_subdirectory(libpuzzle-0.11)

add_library(ThirdParty STATIC
  base64.h base64.cpp
  )

set_target_properties(ThirdParty PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_include_directories(ThirdParty PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# Try to force Leviathan to not use dependencies
option(LEVIATHAN_AS_LIBRARY "DO NOT CHANGE THIS OPTION" ON)
option(BUILD_TESTS "DO NOT CHANGE DISABLES LEVIATHAN TESTS" OFF)
set(LEVIATHAN_AS_LIBRARY ON)
set(BUILD_TESTS OFF)
add_subdirectory(Leviathan)

if(CREATE_DL_PLUGINS)
  add_subdirectory(GQ)
endif()
