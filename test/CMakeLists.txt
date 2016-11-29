
set(CMAKE_INSTALL_RPATH "$ORIGIN/../leviathan:$ORIGIN/../GQ:$ORIGIN/../lib")

set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

include_directories("${PROJECT_SOURCE_DIR}/third_party")

add_executable(Test
  testmain.cpp

  TestDatabase.h

  # Actual tests
  tests.cpp
  test_image.cpp
  test_collection.cpp
  test_helpers.cpp
  test_events.cpp

  gtk_tests.cpp

  db_tests.cpp

  DummyLog.h DummyLog.cpp
  TestDualView.h
  )

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
