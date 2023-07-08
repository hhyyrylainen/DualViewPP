# Core library. Used by the main application and the test to avoid compiling
# twice

if(DISABLE_DEPRECATED_GTK)
  add_definitions(-DGTKMM_DISABLE_DEPRECATED -DGDKMM_DISABLE_DEPRECATED)
endif()

# Configure header
configure_file("Common.h.in" "${PROJECT_BINARY_DIR}/Common.h")
# Make a readonly copy of it
file(COPY "${PROJECT_BINARY_DIR}/Common.h" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}"
  NO_SOURCE_PERMISSIONS
  FILE_PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)


# Resource file packing
file(GLOB_RECURSE ALL_RESOURCE_FILES "${PROJECT_SOURCE_DIR}/resources/*.*")

make_directory("${PROJECT_BINARY_DIR}/generated")

# add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/generated/dualviewpp.gresource.h"
#   COMMAND "glib-compile-resources" "${PROJECT_SOURCE_DIR}/dualviewpp.gresource.xml"
#   "--target=${CMAKE_BINARY_DIR}/generated/dualviewpp.gresource.h"
#   "--sourcedir=${PROJECT_SOURCE_DIR}"
#   "--generate-header"
#   "--c-name=dualview"
#   DEPENDS ../dualviewpp.gresource.xml
#   )

add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/generated/dualviewpp.gresource.cpp"
  COMMAND "glib-compile-resources" "${PROJECT_SOURCE_DIR}/dualviewpp.gresource.xml"
  "--target=${CMAKE_BINARY_DIR}/generated/dualviewpp.gresource.cpp"
  "--sourcedir=${PROJECT_SOURCE_DIR}"
  "--generate-source"
  "--c-name=dualview"
  DEPENDS ../dualviewpp.gresource.xml ${ALL_RESOURCE_FILES}
  )


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
  SignatureCalculator.h SignatureCalculator.cpp
  ReversibleAction.h ReversibleAction.cpp

  IsAlive.h

  VirtualPath.h VirtualPath.cpp
  TimeHelpers.h TimeHelpers.cpp

  TaskListWithPriority.h
  
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
  windows/Reorder.h windows/Reorder.cpp
  windows/ActionEditors.h windows/ActionEditors.cpp
  windows/DownloadItemEditor.h windows/DownloadItemEditor.cpp
  windows/RenameWindow.h windows/RenameWindow.cpp
  windows/AlreadyImportedImageDeleter.h windows/AlreadyImportedImageDeleter.cpp
  windows/MaintenanceTools.h windows/MaintenanceTools.cpp

  # Components for windows
  components/SuperViewer.h components/SuperViewer.cpp
  components/SuperContainer.h components/SuperContainer.cpp
  components/ListItem.h components/ListItem.cpp
  components/ImageListItem.h components/ImageListItem.cpp
  components/CollectionListItem.h components/CollectionListItem.cpp
  components/FolderListItem.h components/FolderListItem.cpp
  components/PrimaryMenu.h components/PrimaryMenu.cpp
  
  components/DLListItem.h components/DLListItem.cpp

  components/TagEditor.h components/TagEditor.cpp
  components/FolderSelector.h components/FolderSelector.cpp

  components/FolderNavigatorHelper.h components/FolderNavigatorHelper.cpp
  components/EasyEntryCompletion.h components/EasyEntryCompletion.cpp
  components/ImageListScroll.h components/ImageListScroll.cpp

  components/ItemDragInformationProvider.h components/ItemDragInformationProvider.cpp  
  
  # Resources
  resources/DatabaseResource.h resources/DatabaseResource.cpp
  resources/ResourceWithPreview.h resources/ResourceWithPreview.cpp
  
  resources/Image.h resources/Image.cpp
  resources/InternetImage.h resources/InternetImage.cpp
  resources/Collection.h resources/Collection.cpp
  resources/Folder.h resources/Folder.cpp
  resources/Tags.h resources/Tags.cpp
  resources/NetGallery.h resources/NetGallery.cpp
  resources/ImagePath.h

  resources/DatabaseAction.h resources/DatabaseAction.cpp

  # Generated headers
  Common.h)

target_link_libraries(Core PUBLIC ThirdParty Engine puzzle
  ${GTKMM_LIBRARIES} ${GTKCANBERRA_LIBRARIES}
  ImageMagick::Magick++ ImageMagick::MagickCore ${Boost_LIBRARIES} ${cryptopp_LIBRARIES} ${SQLITE3_LIBRARIES}
  ${CURL_LIBRARIES} date::date-tz)

target_include_directories(Core PUBLIC ${CMAKE_SOURCE_DIR}/src)

# Main executable
add_executable(dualviewpp main.cpp
  # Embedded resources
  "${PROJECT_BINARY_DIR}/generated/dualviewpp.gresource.cpp"  
  )

target_link_libraries(dualviewpp Core ${CMAKE_THREAD_LIBS_INIT}
  ${CMAKE_DL_LIBS})

set_target_properties(dualviewpp PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
