# Create shared libraries from all the plugins

set(CMAKE_INSTALL_RPATH "$ORIGIN/../leviathan:$ORIGIN/../hcxselect:$ORIGIN/../lib")

set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

pkg_check_modules(GUMBO REQUIRED gumbo)

include_directories(${GUMBO_INCLUDE_DIRS})
link_directories(${GUMBO_LIBRARY_DIRS})

add_library(Plugin_Imgur SHARED Plugin_Imgur.cpp)

target_link_libraries(Plugin_Imgur Leviathan hcxselect ${GUMBO_LIBRARIES} ${CMAKE_DL_LIBS})

add_library(Plugin_GoogleImages SHARED Plugin_GoogleImages.cpp)

target_link_libraries(Plugin_GoogleImages Leviathan hcxselect ${GUMBO_LIBRARIES}
  ${CMAKE_DL_LIBS})


