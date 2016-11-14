# Create shared libraries from all the plugins

set(CMAKE_INSTALL_RPATH "$ORIGIN/../leviathan:$ORIGIN/../GQ:$ORIGIN/../lib")

set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

# Imgur
add_library(Plugin_Imgur SHARED Plugin_Imgur.cpp)

target_link_libraries(Plugin_Imgur Leviathan GQ ${CMAKE_DL_LIBS})

# Google Images
add_library(Plugin_GoogleImages SHARED Plugin_GoogleImages.cpp)

target_link_libraries(Plugin_GoogleImages Leviathan GQ ${CMAKE_DL_LIBS})


