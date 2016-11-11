# Core library. Used by the main application and the test to avoid compiling
# twice
include_directories("${PROJECT_SOURCE_DIR}/core")

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

add_library(Core

  DualView.h DualView.cpp
  Plugin.h PluginManager.h PluginManager.cpp
  
  Settings.h Settings.cpp
  CurlWrapper.h CurlWrapper.cpp

  Database.h Database.cpp
  SQLHelpers.h SQLHelpers.cpp
  PreparedStatement.h PreparedStatement.cpp
  SingleLoad.h 
  CacheManager.h CacheManager.cpp

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
  
  # Resources
  resources/DatabaseResource.h resources/DatabaseResource.cpp
  resources/ResourceWithPreview.h resources/ResourceWithPreview.cpp
  
  resources/Image.h resources/Image.cpp
  resources/Collection.h resources/Collection.cpp
  resources/Folder.h resources/Folder.cpp
  resources/Tags.h resources/Tags.cpp

  # Generated headers
  ${GENERATED_SQL_FILES}
  )


target_link_libraries(Core ThirdParty Leviathan
  ${GTKMM_LIBRARIES} ${GTKCANBERRA_LIBRARIES}
  ${MAGICK_LIBRARIES} ${Boost_LIBRARIES} ${CRYPTOPP_LIBRARIES} ${SQLITE3_LIBRARIES}
  ${CURL_LIBRARIES})

target_compile_options(Core PUBLIC ${MAGICK_CFLAGS_OTHER})
