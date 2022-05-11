#
# Copyright(c) 2006 to 2021 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
find_package(CUnit REQUIRED)

set(CUNIT_DIR "${CMAKE_CURRENT_LIST_DIR}/CUnit")


function(get_cunit_header_file SOURCE_FILE HEADER_FILE)
  # Return a unique (but consistent) filename where Theory macros can be
  # written. The path that will be returned is the location to a header file
  # located in the same relative directory, using the basename of the source
  # file postfixed with .h. e.g. <project>/foo/bar.h would be converted to
  # <project>/build/foo/bar.h.
  get_filename_component(SOURCE_FILE "${SOURCE_FILE}" ABSOLUTE)
  file(RELATIVE_PATH SOURCE_FILE "${PROJECT_SOURCE_DIR}" "${SOURCE_FILE}")
  get_filename_component(basename "${SOURCE_FILE}" NAME_WE)
  get_filename_component(dir "${SOURCE_FILE}" DIRECTORY)
  set(${HEADER_FILE} "${CMAKE_BINARY_DIR}/${dir}/${basename}.h" PARENT_SCOPE)
endfunction()

function(parse_cunit_fixtures INPUT TEST_DISABLED TEST_TIMEOUT)
  if(INPUT MATCHES ".disabled${s}*=${s}*([tT][rR][uU][eE]|[0-9]+)")
    set(${TEST_DISABLED} "TRUE" PARENT_SCOPE)
  else()
    set(${TEST_DISABLED} "FALSE" PARENT_SCOPE)
  endif()

  if(INPUT MATCHES ".timeout${s}*=${s}*([0-9]+)")
    set(${TEST_TIMEOUT} "${CMAKE_MATCH_1}" PARENT_SCOPE)
  else()
    set(${TEST_TIMEOUT} "0" PARENT_SCOPE)
  endif()
endfunction()

# Parse a single source file, generate a header file with theory definitions
# (if applicable) and return suite and test definitions.
function(process_cunit_source_file SOURCE_FILE HEADER_FILE SUITES TESTS)
  set(x "\\*")
  set(s "[ \t\r\n]")
  set(s_or_x "[ \t\r\n\\*]")
  set(w "[_a-zA-Z0-9]")
  set(ident_expr "(${s}*${w}+${s}*)")
  # Very basic type recognition, only things that contain word characters and
  # pointers are handled. And since this script does not actually have to
  # compile any code, type checking is left to the compiler. An error (or
  # warning) will be thrown if something is off.
  #
  # The "type" regular expression below will match things like:
  #  - <word> <word>
  #  - <word> *<word>
  #  - <word>* <word> *<word>
  set(type_expr "(${s}*${w}+${x}*${s}+${s_or_x}*)+")
  set(param_expr "${type_expr}${ident_expr}")
  # Test fixture support (based on test fixtures as implemented in Criterion),
  # to enable per-test (de)initializers, which is very different from
  # per-suite (de)initializers, and timeouts.
  #
  # The following fixtures are supported:
  #  - init
  #  - fini
  #  - disabled
  #  - timeout
  set(data_expr "(${s}*,${s}*\\.${w}+${s}*=[^,\\)]+)*")

  set(suites_wo_init_n_clean)
  set(suites_w_init)
  set(suites_w_clean)

  file(READ "${SOURCE_FILE}" content)

  # CU_Init and CU_Clean
  #
  # Extract all suite initializers and deinitializers so that the list of
  # suites can be probably populated when tests and theories are parsed. Suites
  # are only registered if it contains at least one test or theory.
  set(suite_expr "CU_(Init|Clean)${s}*\\(${ident_expr}\\)")
  string(REGEX MATCHALL "${suite_expr}" matches "${content}")
  foreach(match ${matches})
    string(REGEX REPLACE "${suite_expr}" "\\2" suite "${match}")

    if("${match}" MATCHES "CU_Init")
      list(APPEND suites_w_init ${suite})
    elseif("${match}" MATCHES "CU_Clean")
      list(APPEND suites_w_deinit ${suite})
    endif()
  endforeach()

  # CU_Test
  set(test_expr "CU_Test${s}*\\(${ident_expr},${ident_expr}${data_expr}\\)")
  string(REGEX MATCHALL "${test_expr}" matches "${content}")
  foreach(match ${matches})
    string(REGEX REPLACE "${test_expr}" "\\1" suite "${match}")
    string(REGEX REPLACE "${test_expr}" "\\2" test "${match}")
    # Remove leading and trailing whitespace
    string(STRIP "${suite}" suite)
    string(STRIP "${test}" test)

    # Extract fixtures that must be handled by CMake (.disabled and .timeout).
    parse_cunit_fixtures("${match}" disabled timeout)
    list(APPEND suites_wo_init_n_clean "${suite}")
    list(APPEND tests "${suite}:${test}:${disabled}:${timeout}")
  endforeach()

  # CU_Theory
  #
  # CU_Theory signatures must be recognized in order to generate structures to
  # hold the CU_DataPoints declarations. The generated type is added to the
  # compile definitions and inserted by the preprocessor when CU_TheoryDataPoints
  # is expanded.
  #
  # NOTE: Since not all compilers support pushing function-style definitions
  #       from the command line (CMake will generate a warning too), a header
  #       file is generated instead. A define is pushed and expanded at
  #       compile time. It is included by CUnit/Theory.h.
  get_cunit_header_file("${SOURCE_FILE}" header)

  set(theory_expr "CU_Theory${s}*\\(${s}*\\((${param_expr}(,${param_expr})*)\\)${s}*,${ident_expr},${ident_expr}${data_expr}\\)")
  string(REGEX MATCHALL "${theory_expr}" matches "${content}")
  foreach(match ${matches})
    if(NOT theories)
      # Ensure generated header is truncated before anything is written.
      file(WRITE "${header}" "")
    endif()
    string(REGEX REPLACE "${theory_expr}" "\\1" params "${match}")
    string(REGEX REPLACE "${theory_expr}" "\\7" suite "${match}")
    string(REGEX REPLACE "${theory_expr}" "\\8" test "${match}")
    # Remove leading and trailing whitespace
    string(STRIP "${params}" params)
    string(STRIP "${suite}" suite)
    string(STRIP "${test}" test)
    # Convert parameters from a string to a list
    string(REGEX REPLACE "${s}*,${s}*" ";" params "${params}")

    # Extract fixtures that must be handled by CMake (.disabled and .timeout)
    parse_cunit_fixtures("${match}" disabled timeout)
    list(APPEND suites_wo_init_n_clean "${suite}")
    list(APPEND theories "${suite}:${test}:${disabled}:${timeout}")

    set(sep)
    set(size "CU_TheoryDataPointsSize_${suite}_${test}(datapoints) (")
    set(slice "CU_TheoryDataPointsSlice_${suite}_${test}(datapoints, index) (")
    set(typedef "CU_TheoryDataPointsTypedef_${suite}_${test}() {")
    foreach(param ${params})
      string(
        REGEX REPLACE "(${type_expr})${ident_expr}" "\\3" name "${param}")
      string(
        REGEX REPLACE "(${type_expr})${ident_expr}" "\\1" type "${param}")
      string(STRIP "${name}" name)
      string(STRIP "${type}" type)

      set(slice "${slice}${sep} datapoints.${name}.p[index]")
      if (NOT sep)
        set(size "${size} datapoints.${name}.n")
        set(sep ",")
      endif()
      set(typedef "${typedef} struct { size_t n; ${type} *p; } ${name};")
    endforeach()
    set(typedef "${typedef} }")
    set(slice "${slice} )")
    set(size "${size} )")

    file(APPEND "${header}" "#define ${size}\n")
    file(APPEND "${header}" "#define ${slice}\n")
    file(APPEND "${header}" "#define ${typedef}\n")
  endforeach()

  # Propagate suites, tests and theories extracted from the source file.
  if(suites_wo_init_n_clean)
    list(REMOVE_DUPLICATES suites_wo_init_n_clean)
    list(SORT suites_wo_init_n_clean)
    foreach(suite ${suites_wo_init_n_clean})
      set(init "FALSE")
      set(clean "FALSE")
      if(${suite} IN_LIST suites_w_init)
        set(init "TRUE")
      endif()
      if(${suite} IN_LIST suites_w_deinit)
        set(clean "TRUE")
      endif()

      list(APPEND suites "${suite}:${init}:${clean}")
    endforeach()
  endif()

  if(theories)
    set(${HEADER_FILE} "${header}" PARENT_SCOPE)
  else()
    unset(${HEADER_FILE} PARENT_SCOPE)
  endif()
  set(${SUITES} ${suites} PARENT_SCOPE)
  set(${TESTS} ${tests};${theories} PARENT_SCOPE)
endfunction()

function(set_test_library_paths TEST_NAME)
  if(ENABLE_SHM)
    find_library(ICEORYX_LIB iceoryx_binding_c)
    get_filename_component(ICEORYX_LIB_PATH ${ICEORYX_LIB} DIRECTORY)
  endif ()
  file(TO_NATIVE_PATH "${CUNIT_LIBRARY_DIR}" cudir)
  if(APPLE)
    set_property(
      TEST ${TEST_NAME}
      PROPERTY ENVIRONMENT
      "DYLD_LIBRARY_PATH=${cudir}:${CMAKE_LIBRARY_OUTPUT_DIRECTORY}:${ICEORYX_LIB_PATH}:$ENV{DYLD_LIBRARY_PATH}")
  elseif(WIN32)
    string(REPLACE "/" "\\" cudir "${cudir}")
    string(REPLACE ";" "\\;" paths "$ENV{PATH}")
    set_property(
      TEST ${TEST_NAME}
      PROPERTY ENVIRONMENT
      "PATH=${cudir}\\;${ICEORYX_LIB_PATH}\\;${paths}")
  else()
    set_property(
      TEST ${TEST_NAME}
      PROPERTY ENVIRONMENT
      "LD_LIBRARY_PATH=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}:${ICEORYX_LIB_PATH}:$ENV{LD_LIBRARY_PATH}")
  endif()
endfunction()

function(add_cunit_executable TARGET)
  # Retrieve location of shared libary, which is need to extend the PATH
  # environment variable on Microsoft Windows, so that the operating
  # system can locate the .dll that it was linked against.
  # On macOS, this mechanism is used to set the DYLD_LIBRARY_PATH.
  get_target_property(CUNIT_LIBRARY_TYPE CUnit TYPE)
  get_target_property(CUNIT_IMPORTED_LOCATION CUnit IMPORTED_LOCATION)
  get_filename_component(CUNIT_LIBRARY_DIR "${CUNIT_IMPORTED_LOCATION}" PATH)

  set(decls)
  set(defns)
  set(sources)

  foreach(source ${ARGN})
    if((EXISTS "${source}" OR EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source}"))
      unset(suites)
      unset(tests)

      process_cunit_source_file("${source}" header suites tests)
      if(header)
        set_property(
          SOURCE "${source}"
          PROPERTY COMPILE_DEFINITIONS CU_THEORY_INCLUDE_FILE=\"${header}\")
      endif()

      # Disable missing-field-initializers warnings as not having to specify
      # every member, aka fixture, is intended behavior.
      if(${CMAKE_C_COMPILER_ID} STREQUAL "Clang" OR
         ${CMAKE_C_COMPILER_ID} STREQUAL "AppleClang" OR
         ${CMAKE_C_COMPILER_ID} STREQUAL "GNU")
        set_property(
          SOURCE "${source}"
          PROPERTY COMPILE_FLAGS -Wno-missing-field-initializers)
      endif()

      foreach(suite ${suites})
        string(REPLACE ":" ";" suite ${suite})
        list(GET suite 2 clean)
        list(GET suite 1 init)
        list(GET suite 0 suite)
        set(init_func "NULL")
        set(clean_func "NULL")
        if(init)
          set(decls "${decls}\nCU_InitDecl(${suite});")
          set(init_func "CU_InitName(${suite})")
        endif()
        if(clean)
          set(decls "${decls}\nCU_CleanDecl(${suite});")
          set(clean_func "CU_CleanName(${suite})")
        endif()
        set(defns "${defns}\nADD_SUITE(${suite}, ${init_func}, ${clean_func});")
      endforeach()

      foreach(test ${tests})
        string(REPLACE ":" ";" test ${test})
        list(GET test 3 timeout)
        list(GET test 2 disabled)
        list(GET test 0 suite)
        list(GET test 1 test)

        set(enable "true")
        if(disabled)
          set(enable "false")
        endif()
        if(NOT timeout)
          set(timeout 10)
        endif()

        set(decls "${decls}\nCU_TestDecl(${suite}, ${test});")
        set(defns "${defns}\nADD_TEST(${suite}, ${test}, ${enable});")
        set(ctest "CUnit_${suite}_${test}")

        add_test(
          NAME ${ctest}
          COMMAND ${TARGET} -s ${suite} -t ${test})
        set_property(TEST ${ctest} PROPERTY TIMEOUT ${timeout})
        set_property(TEST ${ctest} PROPERTY DISABLED ${disabled})
        set_test_library_paths(${ctest})

      endforeach()

      list(APPEND sources "${source}")
    endif()
  endforeach()

  configure_file(
    "${CUNIT_DIR}/src/main.c.in" "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.c" @ONLY)
  if("2.1.3" VERSION_LESS_EQUAL
       "${CUNIT_VERSION_MAJOR}.${CUNIT_VERSION_MINOR}.${CUNIT_VERSION_PATCH}")
    set_property(
      SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.c"
      PROPERTY COMPILE_DEFINITIONS HAVE_ENABLE_JUNIT_XML)
  endif()

  add_executable(
    ${TARGET} "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.c" ${sources})
  target_link_libraries(${TARGET} PRIVATE CUnit)
  target_include_directories(${TARGET} PRIVATE "${CUNIT_DIR}/include")
  if(MSVC)
    target_compile_definitions(${TARGET} PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
endfunction()

