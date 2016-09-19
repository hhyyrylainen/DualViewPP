
pkg_check_modules(HTMLCXX REQUIRED htmlcxx)

include_directories(${HTMLCXX_INCLUDE_DIRS})
link_directories(${HTMLCXX_LIBRARY_DIRS})

add_library(hcxselect hcxselect.h hcxselect.cpp lexer.h)

target_link_libraries(hcxselect ${HTMLCXX_LIBRARIES})
  
