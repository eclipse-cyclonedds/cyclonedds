set(CMAKE_MODULE_PATH "${MAIN_PROJECT_DIR}/cmake/Modules")

include(HashUtilities)

# Path we need to strip out of parser.{c,h}
string(REGEX REPLACE "[-+:/\\=()]" "_" _path_to_tok ${binary_dir})
string(TOUPPER "${_path_to_tok}" _path_to_tok)

# Strip out the paths of generated files
filter_files(
  FIND "${_path_to_tok}"
  REPLACE ""
  FILES
    ${binary_dir}/parser.c
    ${binary_dir}/parser.h
)
filter_files(
  FIND "${binary_dir}/"
  REPLACE ""
  FILES
    ${binary_dir}/parser.c
    ${binary_dir}/parser.h
)
filter_files(
  FIND "${source_dir}/"
  REPLACE ""
  FILES
    ${binary_dir}/parser.c
    ${binary_dir}/parser.h
)

foreach(_parser_file ${binary_dir}/parser.c ${binary_dir}/parser.h)
  file(READ "${_parser_file}" _parser_file_data)
  string(REGEX REPLACE
    "\n?/\\* generated from parser\\.y\\[[0-9a-fA-F]+\\] \\*/\n?"
    "\n"
    _parser_file_data
    "${_parser_file_data}")
  file(WRITE "${_parser_file}" "${_parser_file_data}")
endforeach()

append_hashes(
    PREFIX "/*"
    POSTFIX "*/"
    HASH_FILES
      ${source_dir}/src/parser.y
    APPEND_FILES
      ${binary_dir}/parser.c
      ${binary_dir}/parser.h
)

file(COPY ${binary_dir}/parser.c DESTINATION ${source_dir}/src)
file(COPY ${binary_dir}/parser.h DESTINATION ${source_dir}/src)
file(COPY ${binary_dir}/parser.c DESTINATION ${binary_dir}/src)
file(COPY ${binary_dir}/parser.h DESTINATION ${binary_dir}/src)
