
# For date library
# add_definitions(-DINSTALL=.)

set(USE_SYSTEM_TZ_DB ON CACHE BOOL "Use the operating system's timezone database")
set(ENABLE_DATE_TESTING OFF CACHE BOOL "Enable unit tests")
set(BUILD_TZ_LIB ON)
add_subdirectory(date)

target_compile_definitions(date INTERFACE HAS_UNCAUGHT_EXCEPTIONS=1)
target_compile_definitions(date-tz INTERFACE HAS_UNCAUGHT_EXCEPTIONS=1)

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
