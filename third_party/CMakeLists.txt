
add_definitions(-DINSTALL=.)

add_library(ThirdParty
  base64.h base64.cpp

  date/tz.h date/tz.cpp
  )
