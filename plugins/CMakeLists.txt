# Create shared libraries from all the plugins

set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib:$ORIGIN/../ThirdParty/Leviathan/bin:$ORIGIN/../ThirdParty/GQ")

set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

# Imgur
add_library(Plugin_Imgur SHARED Plugin_Imgur.cpp)

target_link_libraries(Plugin_Imgur Core Engine GQ ${CMAKE_DL_LIBS})


#add_library(Plugin_NEW SHARED Plugin_NEW.cpp)
#target_link_libraries(Plugin_NEW Engine GQ ${CMAKE_DL_LIBS})


