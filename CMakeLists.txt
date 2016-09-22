cmake_minimum_required(VERSION 2.8.3)

project(DualView++)

option(CREATE_DL_PLUGINS "Set to OFF to skip building web download tools" ON)

set(DUALVIEW_VERSION_STR "0.0.1")

if(CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
    "Set the build type, usually Release or RelWithDebInfo" FORCE)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

if(WIN32)
  
  # program database flag for debug
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /ZI -Gm /W3")
  
else(WIN32)

  # Has to be on one line or else ';'s will be included
  # C++14
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wno-unused-function -Wno-unused-variable -Wno-unknown-pragmas -Wl,--no-undefined -Wl,--no-allow-shlib-undefined -Wno-pragma-once-outside-header")
  
endif(WIN32)

# Find boost
find_package(Boost REQUIRED COMPONENTS filesystem)

find_package(CryptoPP REQUIRED)

include_directories(${CRYPTOPP_INCLUDE_DIRS})

include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

# Find Gtk
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)

include_directories(${GTKMM_INCLUDE_DIRS})
link_directories(${GTKMM_LIBRARY_DIRS})

# This is not in the default ImageMagick-devel package.
# it is in: ImageMagick-c++-devel
pkg_check_modules(MAGICK REQUIRED Magick++)

include_directories(${MAGICK_INCLUDE_DIRS})
link_directories(${MAGICK_LIBRARY_DIRS})

# Default system threads
find_package(Threads)

include_directories(leviathan)
include_directories("${PROJECT_SOURCE_DIR}")

# Configure header
configure_file("${PROJECT_SOURCE_DIR}/Common.h.in" "${PROJECT_SOURCE_DIR}/Common.h")

add_subdirectory(leviathan)
add_subdirectory(third_party)

if(CREATE_DL_PLUGINS)
  add_subdirectory(hcxselect)
  add_subdirectory(plugins)
endif()

add_subdirectory(core)

add_executable(dualviewpp
  main.cpp

  Common.h
  )

target_link_libraries(dualviewpp Core ${GTKMM_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
  ${CMAKE_DL_LIBS})


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

# Testing
add_subdirectory(test)

if(UNIX)
    add_custom_target(check COMMAND "${PROJECT_BINARY_DIR}/test/Test"
    DEPENDS Test
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/test")
endif()


