cmake_minimum_required(VERSION 2.8.3)

project(DualView++)

set(DUALVIEW_VERSION_STR "0.0.1")

if(CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
    "Set the build type, usually Release or RelWithDebInfo" FORCE)
endif()

if(WIN32)
  
  # program database flag for debug
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /ZI -Gm /W3")
  
else(WIN32)

  # Has to be on one line or else ';'s will be included
  # C++14
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wno-unused-function -Wno-unused-variable -Wno-unknown-pragmas -Wl,--no-undefined -Wl,--no-allow-shlib-undefined -Wno-pragma-once-outside-header")
  
endif(WIN32)

# Find Gtk
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)

include_directories(${GTKMM_INCLUDE_DIRS})
link_directories(${GTKMM_LIBRARY_DIRS})

# Default system threads
find_package(Threads)

include_directories(leviathan)

# Configure header
configure_file("${PROJECT_SOURCE_DIR}/Common.h.in" "${PROJECT_SOURCE_DIR}/Common.h")

add_subdirectory(leviathan)

add_executable(dualviewpp
  main.cpp
  DualView.h DualView.cpp
  Common.h

  )

target_link_libraries(dualviewpp ${GTKMM_LIBRARIES} Leviathan ${CMAKE_THREAD_LIBS_INIT})


# Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)

  if(DOCUMENTATION_LOCAL)
    set(DOXYGEN_SERVER_SEARCH "FALSE")
  else()
    set(DOXYGEN_SERVER_SEARCH "TRUE")
  endif()
  
  configure_file("${PROJECT_SOURCE_DIR}/DoxyFile.in"
    "${PROJECT_BINARY_DIR}/DoxyFile" @ONLY)
  add_custom_target(doc
    ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/DoxyFile
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen" VERBATIM
    )
  
endif(DOXYGEN_FOUND)

