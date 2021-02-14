
set(CMAKE_INSTALL_RPATH "$ORIGIN/../leviathan:$ORIGIN/../GQ:$ORIGIN/../lib")

set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib:$ORIGIN/../ThirdParty/Leviathan/bin")

add_executable(Test
  testmain.cpp

  TestDatabase.h

  # Actual tests
  tests.cpp
  test_image.cpp
  test_collection.cpp
  test_helpers.cpp
  test_events.cpp
  test_undo.cpp
  test_net_gallery.cpp
  test_task_list.cpp
  test_search.cpp
  test_folder.cpp

  gtk_tests.cpp

  db_tests.cpp
  test_actions.cpp

  DummyLog.h DummyLog.cpp
  TestDualView.h

  # Generated resource file
  # TODO: it might be nice to have a separate resources bundle for the tests and it
  # would be nice to keep the separate bundles up to date with cmake
  "${PROJECT_BINARY_DIR}/generated/dualviewpp.gresource.cpp")

set_source_files_properties("${PROJECT_BINARY_DIR}/generated/dualviewpp.gresource.cpp"
  PROPERTIES GENERATED TRUE)

target_include_directories(Test PRIVATE "${PROJECT_BINARY_DIR}/generated")

# For file generation
add_dependencies(Test dualviewpp)

target_link_libraries(Test Core
  # Need to be linked to same libraries as dualviewpp executable
  ${CMAKE_THREAD_LIBS_INIT}
  ${CMAKE_DL_LIBS}
  ${SQLITE3_LIBRARIES}
  )

# Create symbolic link for test data
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/test")
# and copy it
file(COPY data DESTINATION "${PROJECT_BINARY_DIR}/test")
