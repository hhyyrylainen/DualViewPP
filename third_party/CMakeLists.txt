
# For date library
# add_definitions(-DINSTALL=.)
SET(ENABLE_DATE_TESTING OFF CACHE BOOL "Enable unit tests for date")
add_subdirectory(date)

add_library(ThirdParty
  base64.h base64.cpp
  )
