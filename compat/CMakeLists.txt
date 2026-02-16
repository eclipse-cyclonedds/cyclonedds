include(CheckIncludeFile)

check_include_file(getopt.h HAVE_GETOPT_H)
if(WIN32 OR NOT HAVE_GETOPT_H)
  add_library(compat OBJECT "${CMAKE_CURRENT_SOURCE_DIR}/getopt.c" "${CMAKE_CURRENT_SOURCE_DIR}/getopt.h")
  target_include_directories(compat PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
else()
  add_library(compat INTERFACE)
endif()

add_library(${PROJECT_NAME}::compat ALIAS compat)
