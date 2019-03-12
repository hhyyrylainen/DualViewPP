
# For date library
# add_definitions(-DINSTALL=.)
set(ENABLE_DATE_TESTING OFF CACHE BOOL "Enable unit tests for date")
add_subdirectory(date)
add_subdirectory(libpuzzle-0.11)

add_library(ThirdParty STATIC
  base64.h base64.cpp
  )

set_target_properties(ThirdParty PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_include_directories(ThirdParty PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

set(LEVIATHAN_AS_LIBRARY ON)
set(BUILD_TESTS OFF)
add_subdirectory(Leviathan)

if(CREATE_DL_PLUGINS)
  add_subdirectory(GQ)
endif()
