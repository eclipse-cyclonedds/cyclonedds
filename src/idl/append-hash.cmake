message("creating hash and writing to disk")

# Hashing routine, remove \r to not get confused on windows if git changes the line endings.
file(READ "${CMAKE_CURRENT_LIST_DIR}/src/parser.y" _parser_y_data)
string(REPLACE "\r" "" _parser_y_data "${_parser_y_data}")
string(SHA1 _parser_y_hash "${_parser_y_data}")

# Store hash
file(WRITE "${CMAKE_CURRENT_LIST_DIR}/src/parser.y.hash" "${_parser_y_hash}")

# Write back cleaned and hash-appended parser.{c,h}
string(REGEX REPLACE "[-+:/\\=()]" "_" _path_to_tok "${CMAKE_CURRENT_SOURCE_DIR}")
string(TOUPPER "${_path_to_tok}" _path_to_tok)

message(warning "ldldl ${_path_to_tok}")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/parser.h" _parser_h_data)
string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" _parser_h_data "${_parser_h_data}")
string(REPLACE "${_path_to_tok}" "" _parser_h_data "${_parser_h_data}")
string(APPEND _parser_h_data "\n\n/* generated from parser.y[${_parser_y_hash}]*/\n")
file(WRITE "${CMAKE_CURRENT_LIST_DIR}/src/parser.h" "${_parser_h_data}")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/parser.c" _parser_c_data)
string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" _parser_c_data "${_parser_c_data}")
string(REPLACE "${_path_to_tok}" "" _parser_c_data "${_parser_c_data}")
string(APPEND _parser_c_data "\n\n/* generated from parser.y[${_parser_y_hash}]*/\n")
file(WRITE "${CMAKE_CURRENT_LIST_DIR}/src/parser.c" "${_parser_c_data}")
