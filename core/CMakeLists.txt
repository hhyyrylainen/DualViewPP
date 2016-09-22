# Core library. Used by the main application and the test to avoid compiling
# twice
include_directories("${PROJECT_SOURCE_DIR}/core")

add_library(Core

  DualView.h DualView.cpp
  Plugin.h PluginManager.h PluginManager.cpp

  # Window stuff
  windows/BaseWindow.h windows/BaseWindow.cpp
  windows/SingleView.h windows/SingleView.cpp

  # Components for windows
  components/SuperViewer.h components/SuperViewer.cpp

  
  Database.cpp

  CacheManager.h CacheManager.cpp
  
  # Resources
  resources/Image.h resources/Image.cpp
  )


target_link_libraries(Core ThirdParty Leviathan
  ${MAGICK_LIBRARIES} ${Boost_LIBRARIES} ${CRYPTOPP_LIBRARIES})

target_compile_options(Core PUBLIC ${MAGICK_CFLAGS_OTHER})
