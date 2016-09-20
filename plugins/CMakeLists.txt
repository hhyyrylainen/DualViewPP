# Create shared libraries from all the plugins

pkg_check_modules(GUMBO REQUIRED gumbo)

include_directories(${GUMBO_INCLUDE_DIRS})
link_directories(${GUMBO_LIBRARY_DIRS})

add_library(Plugin_Imgur SHARED Plugin_Imgur.cpp)

target_link_libraries(Plugin_Imgur hcxselect ${GUMBO_LIBRARIES} ${CMAKE_DL_LIBS})


