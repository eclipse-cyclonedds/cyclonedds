set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../cmake/Modules")

message("${CMAKE_CURRENT_LIST_DIR}/../../cmake/Modules")

include(HashUtilities)

# Path we need to strip out of parser.{c,h}
string(REGEX REPLACE "[-+:/\\=()]" "_" _path_to_tok "${CMAKE_CURRENT_BINARY_DIR}")
string(TOUPPER "${_path_to_tok}" _path_to_tok)

# Strip out the paths of generated files
filter_files(
  FIND "${_path_to_tok}"
  REPLACE ""
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/parser.c"
    "${CMAKE_CURRENT_BINARY_DIR}/parser.h"
)
filter_files(
  FIND "${CMAKE_CURRENT_BINARY_DIR}/"
  REPLACE ""
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/parser.c"
    "${CMAKE_CURRENT_BINARY_DIR}/parser.h"
)

# ... that we can then copy into place and append a hash
file(COPY "${CMAKE_CURRENT_BINARY_DIR}/parser.c" DESTINATION "${CMAKE_CURRENT_LIST_DIR}/src/")
file(COPY "${CMAKE_CURRENT_BINARY_DIR}/parser.h" DESTINATION "${CMAKE_CURRENT_LIST_DIR}/src/")
append_hashes(
  PREFIX "/*"
  POSTFIX "*/"
  HASH_FILES
    "${CMAKE_CURRENT_LIST_DIR}/src/parser.y"
  APPEND_FILES
    "${CMAKE_CURRENT_LIST_DIR}/src/parser.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/parser.h"
)