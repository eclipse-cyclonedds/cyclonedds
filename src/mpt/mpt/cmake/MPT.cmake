#
# Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
set(MPT_DIR "${CMAKE_CURRENT_LIST_DIR}/..")



function(parse_mpt_fixtures INPUT TEST_DISABLED TEST_TIMEOUT TEST_XFAIL)
  set(s "[ \t\r\n]")
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

  if(INPUT MATCHES ".xfail${s}*=${s}*([tT][rR][uU][eE]|[0-9]+)")
    set(${TEST_XFAIL} "true" PARENT_SCOPE)
  else()
    set(${TEST_XFAIL} "false" PARENT_SCOPE)
  endif()
endfunction()



function(process_mpt_source_file SOURCE_FILE SUITES TESTS PROCESSES)
  unset(tests)
  unset(processes)
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

  file(READ "${SOURCE_FILE}" content)

  # MPT_Test
  set(test_expr "MPT_Test${s}*\\(${ident_expr},${ident_expr}${data_expr}\\)")
  string(REGEX MATCHALL "${test_expr}" matches "${content}")
  foreach(match ${matches})
    string(REGEX REPLACE "${test_expr}" "\\1" suite "${match}")
    string(REGEX REPLACE "${test_expr}" "\\2" test "${match}")
    # Remove leading and trailing whitespace
    string(STRIP "${suite}" suite)
    string(STRIP "${test}" test)

    # Extract fixtures that must be handled by CMake (.disabled and .timeout).
    parse_mpt_fixtures("${match}" disabled timeout xfail)
    list(APPEND tests "${suite}:${test}:${disabled}:${timeout}:${xfail}")
    list(APPEND suites "${suite}")
  endforeach()

  # MPT_TestProcess
  set(process_expr "MPT_TestProcess${s}*\\(${ident_expr},${ident_expr},${ident_expr}")
  string(REGEX MATCHALL "${process_expr}" matches "${content}")
  foreach(match ${matches})
    string(REGEX REPLACE "${process_expr}" "\\1" suite "${match}")
    string(REGEX REPLACE "${process_expr}" "\\2" test "${match}")
    string(REGEX REPLACE "${process_expr}" "\\3" id "${match}")
    # Remove leading and trailing whitespace
    string(STRIP "${suite}" suite)
    string(STRIP "${test}" test)
    string(STRIP "${id}" id)

    list(APPEND processes "${suite}:${test}:${id}")
  endforeach()

  set(${PROCESSES} ${processes} PARENT_SCOPE)
  set(${TESTS} ${tests} PARENT_SCOPE)
  set(${SUITES} ${suites} PARENT_SCOPE)
endfunction()



function(add_mpt_executable TARGET)
  set(sources)
  set(processes)

  foreach(source ${ARGN})
    if((EXISTS "${source}" OR EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source}"))
      unset(processes)
      unset(tests)
      unset(suites)

      # Disable missing-field-initializers warnings as not having to specify
      # every member, aka fixture, is intended behavior.
      if(${CMAKE_C_COMPILER_ID} STREQUAL "Clang" OR
         ${CMAKE_C_COMPILER_ID} STREQUAL "AppleClang" OR
         ${CMAKE_C_COMPILER_ID} STREQUAL "GNU")
        set_property(
          SOURCE "${source}"
          PROPERTY COMPILE_FLAGS -Wno-missing-field-initializers)
      endif()

      process_mpt_source_file("${source}" suites tests processes)

      foreach(suite ${suites})
        set(addsuites "${addsuites}\n  mpt_add_suite(mpt_suite_new(\"${suite}\"));")
      endforeach()

      foreach(testcase ${tests})
        string(REPLACE ":" ";" testcase ${testcase})
        list(GET testcase 0 suite)
        list(GET testcase 1 test)
        list(GET testcase 2 disabled)
        list(GET testcase 3 timeout)
        list(GET testcase 4 xfail)
        set(addtests "${addtests}\n  mpt_add_test(\"${suite}\", mpt_test_new(\"${test}\", ${timeout}, ${xfail}));")

        # Add this test to ctest.
        set(ctest "${TARGET}_${suite}_${test}")
        add_test(
          NAME ${ctest}
          COMMAND ${TARGET} -s ${suite} -t ${test})
        set_property(TEST ${ctest} PROPERTY TIMEOUT ${timeout})
        set_property(TEST ${ctest} PROPERTY DISABLED ${disabled})
      endforeach()

      foreach(process ${processes})
        string(REPLACE ":" ";" process ${process})
        list(GET process 0 suite)
        list(GET process 1 test)
        list(GET process 2 id)
        set(addprocs "${addprocs}\n  mpt_add_process(\"${suite}\", \"${test}\", mpt_process_new(\"${id}\", MPT_TestProcessName(${suite}, ${test}, ${id})));")
        set(procdecls "${procdecls}\nextern MPT_TestProcessDeclaration(${suite}, ${test}, ${id});")
      endforeach()

      list(APPEND sources "${source}")
    endif()
  endforeach()

  configure_file(
    "${MPT_DIR}/src/main.c.in" "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.c" @ONLY)

  add_executable(
    ${TARGET} "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.c" ${sources})

  target_include_directories(${TARGET} PRIVATE "${MPT_DIR}/include" "${MPT_BINARY_ROOT_DIR}/mpt/include")
  target_link_libraries(${TARGET} PRIVATE ddsc)
endfunction()

