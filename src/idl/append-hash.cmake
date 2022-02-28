message("creating hash and writing to disk")

# Hashing routine, remove \r to not get confused on windows if git changes the line endings.
file(READ "${CMAKE_CURRENT_LIST_DIR}/src/parser.y" _parser_y_data)
string(REPLACE "\r" "" _parser_y_data "${_parser_y_data}")
string(SHA1 _parser_y_hash "${_parser_y_data}")

# Store hash
file(WRITE "${CMAKE_CURRENT_LIST_DIR}/src/parser.y.hash" "${_parser_y_hash}")
file(COPY
  "${CMAKE_CURRENT_SOURCE_DIR}/parser.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/parser.c"
  DESTINATION "${CMAKE_CURRENT_LIST_DIR}/src")
file(APPEND "${CMAKE_CURRENT_LIST_DIR}/src/parser.h" "\n\n/* generated from parser.y[${_parser_y_hash}]*/\n")
file(APPEND "${CMAKE_CURRENT_LIST_DIR}/src/parser.c" "\n\n/* generated from parser.y[${_parser_y_hash}]*/\n")
