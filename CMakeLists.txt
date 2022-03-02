cmake_minimum_required(VERSION 3.0)

cmake_policy(SET CMP0077 NEW)

set(DUALVIEW_VERSION_STR "1.0.0")

project(DualView++ VERSION ${DUALVIEW_VERSION_STR})

option(CREATE_DL_PLUGINS "Set to OFF to skip building web download tools" ON)

option(USE_GPROF "Set to ON to use gprof profiling" OFF)

option(DISABLE_DEPRECATED_GTK "Set to OFF to allow use of deprecated GTKMM APIs" ON)

option(DOCUMENTATION_LOCAL "If OFF php search is included in the documentation" ON)

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
  # C++17
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Wno-unused-function -Wno-unused-variable -Wno-unknown-pragmas -Wl,--no-undefined -Wl,--no-allow-shlib-undefined -Wno-pragma-once-outside-header")

  if(USE_GPROF)
    # Enabling profiling
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
  endif()

  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions("-D_DEBUG")
  endif()

  # Setup rpath
  set(CMAKE_INSTALL_RPATH "$ORIGIN/lib:$ORIGIN/ThirdParty/Leviathan/bin:$ORIGIN/ThirdParty/GQ")

  set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
  
endif(WIN32)

# Needed for date library
add_compile_definitions(HAS_UNCAUGHT_EXCEPTIONS=1)

# Find boost
find_package(Boost REQUIRED COMPONENTS filesystem locale)

if(NOT Boost_FOUND)
  message(SEND_ERROR "boost libraries not found")
endif()

include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})


find_package(cryptopp REQUIRED)

if(NOT cryptopp_FOUND)
  message(SEND_ERROR "Crypto++ not found")
endif()

include_directories(${cryptopp_INCLUDE_DIRS})


find_package(CURL REQUIRED)

if(NOT CURL_FOUND)
  message(SEND_ERROR "CURL not found")
endif()

include_directories(${CURL_INCLUDE_DIRS})


# Find Gtk
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)

include_directories(${GTKMM_INCLUDE_DIRS})
link_directories(${GTKMM_LIBRARY_DIRS})

# Gtk canberra
pkg_check_modules(GTKCANBERRA REQUIRED libcanberra-gtk3)

include_directories(${GTKCANBERRA_INCLUDE_DIRS})
link_directories(${GTKCANBERRA_LIBRARY_DIRS})

# This is not in the default ImageMagick-devel package.
# it is in: ImageMagick-c++-devel
pkg_check_modules(MAGICK REQUIRED Magick++)

include_directories(${MAGICK_INCLUDE_DIRS})
link_directories(${MAGICK_LIBRARY_DIRS})


pkg_check_modules(SQLITE3 REQUIRED sqlite3)

include_directories(${SQLITE3_INCLUDE_DIRS})

# end of system dependencies

include_directories("${PROJECT_SOURCE_DIR}")

add_subdirectory(ThirdParty)

if(CREATE_DL_PLUGINS)
  add_subdirectory(plugins)
endif()

# Default system threads
find_package(Threads)

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


add_subdirectory(src)

# Testing
add_subdirectory(test)

if(UNIX)
    add_custom_target(check COMMAND "${PROJECT_BINARY_DIR}/test/Test"
    DEPENDS Test
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/test")

  add_custom_target(check-expensive COMMAND "${PROJECT_BINARY_DIR}/test/Test" "[.expensive]"
    DEPENDS Test
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/test")
endif()


