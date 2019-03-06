# Core library. Used by the main application and the test to avoid compiling
# twice

# Configure header
configure_file("Common.h.in" "${PROJECT_BINARY_DIR}/Common.h")
# Make a readonly copy of it
file(COPY "${PROJECT_BINARY_DIR}/Common.h" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}"
  NO_SOURCE_PERMISSIONS
  FILE_PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)

# Sql file generation
set(GENERATED_SQL_FILES)
make_directory("${PROJECT_SOURCE_DIR}/generated")

macro(add_sql_generated file)

  list(APPEND GENERATED_SQL_FILES "${PROJECT_SOURCE_DIR}/generated/${file}.h")
  
  add_custom_command(OUTPUT "${PROJECT_SOURCE_DIR}/generated/${file}.h"
    COMMAND "ruby" "${PROJECT_SOURCE_DIR}/generator/SqlToC.rb"
    "${PROJECT_SOURCE_DIR}/sql/${file}" "${PROJECT_SOURCE_DIR}/generated/${file}.h"
    DEPENDS "${PROJECT_SOURCE_DIR}/generator/SqlToC.rb" "${PROJECT_SOURCE_DIR}/sql/${file}"
    )

endmacro(add_sql_generated)

add_sql_generated("maintables.sql")
add_sql_generated("defaulttablevalues.sql")
add_sql_generated("defaulttags.sql")
add_sql_generated("migration_14_15.sql")
add_sql_generated("migration_15_16.sql")
add_sql_generated("migration_16_17.sql")
add_sql_generated("migration_18_19.sql")

add_library(Core

  DualView.h DualView.cpp
  Plugin.h PluginManager.h PluginManager.cpp
  
  Settings.h Settings.cpp
  CurlWrapper.h CurlWrapper.cpp

  Database.h Database.cpp
  ChangeEvents.h ChangeEvents.cpp
  SQLHelpers.h SQLHelpers.cpp
  UtilityHelpers.h UtilityHelpers.cpp
  PreparedStatement.h PreparedStatement.cpp
  SingleLoad.h 
  CacheManager.h CacheManager.cpp
  DownloadManager.h DownloadManager.cpp

  IsAlive.h
  UbuntuWorkaround.h

  VirtualPath.h VirtualPath.cpp
  TimeHelpers.h TimeHelpers.cpp
  
  # Window stuff
  windows/BaseWindow.h windows/BaseWindow.cpp
  windows/SingleView.h windows/SingleView.cpp
  windows/CollectionView.h windows/CollectionView.cpp
  windows/Importer.h windows/Importer.cpp
  windows/TagManager.h windows/TagManager.cpp
  windows/FolderCreator.h windows/FolderCreator.cpp
  windows/SingleCollection.h windows/SingleCollection.cpp
  windows/Downloader.h windows/Downloader.cpp
  windows/DownloadSetup.h windows/DownloadSetup.cpp
  windows/AddToFolder.h windows/AddToFolder.cpp
  windows/RemoveFromFolders.h windows/RemoveFromFolders.cpp
  windows/DebugWindow.h windows/DebugWindow.cpp
  windows/ImageFinder.h windows/ImageFinder.cpp
  windows/Undo.h windows/Undo.cpp
  windows/DuplicateFinder.h windows/DuplicateFinder.cpp

  # Components for windows
  components/SuperViewer.h components/SuperViewer.cpp
  components/SuperContainer.h components/SuperContainer.cpp
  components/ListItem.h components/ListItem.cpp
  components/ImageListItem.h components/ImageListItem.cpp
  components/CollectionListItem.h components/CollectionListItem.cpp
  components/FolderListItem.h components/FolderListItem.cpp

  components/DLListItem.h components/DLListItem.cpp

  components/TagEditor.h components/TagEditor.cpp
  components/FolderSelector.h components/FolderSelector.cpp

  components/FolderNavigatorHelper.h components/FolderNavigatorHelper.cpp
  components/EasyEntryCompletion.h components/EasyEntryCompletion.cpp
  components/ImageListScroll.h components/ImageListScroll.cpp
  
  # Resources
  resources/DatabaseResource.h resources/DatabaseResource.cpp
  resources/ResourceWithPreview.h resources/ResourceWithPreview.cpp
  
  resources/Image.h resources/Image.cpp
  resources/InternetImage.h resources/InternetImage.cpp
  resources/Collection.h resources/Collection.cpp
  resources/Folder.h resources/Folder.cpp
  resources/Tags.h resources/Tags.cpp
  resources/NetGallery.h resources/NetGallery.cpp

  # Generated headers
  ${GENERATED_SQL_FILES}

  Common.h
  )


target_link_libraries(Core ThirdParty tz Engine
  ${GTKMM_LIBRARIES} ${GTKCANBERRA_LIBRARIES}
  ${MAGICK_LIBRARIES} ${Boost_LIBRARIES} ${CRYPTOPP_LIBRARIES} ${SQLITE3_LIBRARIES}
  ${CURL_LIBRARIES})

target_compile_options(Core PUBLIC ${MAGICK_CFLAGS_OTHER})
target_include_directories(Core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# Main executable
add_executable(dualviewpp main.cpp)

target_link_libraries(dualviewpp Core ${CMAKE_THREAD_LIBS_INIT}
  ${CMAKE_DL_LIBS})

set_target_properties(dualviewpp PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
