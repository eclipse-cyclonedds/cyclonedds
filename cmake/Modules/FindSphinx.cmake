#
# Copyright(c) 2019 Jeroen Koekkoek
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
include(FindPackageHandleStandardArgs)

macro(_Sphinx_find_executable _exe)
  string(TOUPPER "${_exe}" _uc)
  # sphinx-(build|quickstart)-3 x.x.x
  # FIXME: This works on Fedora (and probably most other UNIX like targets).
  #        Windows targets and PIP installs might need some work.
  find_program(
    SPHINX_${_uc}_EXECUTABLE
    NAMES "sphinx-${_exe}-3" "sphinx-${_exe}" "sphinx-${_exe}.exe")

  if(SPHINX_${_uc}_EXECUTABLE)
    execute_process(
      COMMAND "${SPHINX_${_uc}_EXECUTABLE}" --version
      RESULT_VARIABLE _result
      OUTPUT_VARIABLE _output
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_result EQUAL 0 AND _output MATCHES " v?([0-9]+\\.[0-9]+\\.[0-9]+)$")
      set(SPHINX_${_uc}_VERSION "${CMAKE_MATCH_1}")
    endif()

    if(NOT TARGET Sphinx::${_exe})
      add_executable(Sphinx::${_exe} IMPORTED GLOBAL)
      set_target_properties(Sphinx::${_exe} PROPERTIES
        IMPORTED_LOCATION "${SPHINX_${_uc}_EXECUTABLE}")
    endif()
    set(Sphinx_${_exe}_FOUND TRUE)
  else()
    set(Sphinx_${_exe}_FOUND FALSE)
  endif()
  unset(_uc)
endmacro()

macro(_Sphinx_find_extension _ext)
  if(_SPHINX_PYTHON_EXECUTABLE)
    execute_process(
      COMMAND ${_SPHINX_PYTHON_EXECUTABLE} -c "import ${_ext}"
      RESULT_VARIABLE _result)
    if(_result EQUAL 0)
      set(Sphinx_${_ext}_FOUND TRUE)
    else()
      set(Sphinx_${_ext}_FOUND FALSE)
    endif()
  elseif(CMAKE_HOST_WIN32 AND SPHINX_BUILD_EXECUTABLE)
    # script-build on Windows located under (when PIP is used):
    # C:/Program Files/PythonXX/Scripts
    # C:/Users/username/AppData/Roaming/Python/PythonXX/Sripts
    #
    # Python modules are installed under:
    # C:/Program Files/PythonXX/Lib
    # C:/Users/username/AppData/Roaming/Python/PythonXX/site-packages
    #
    # To verify a given module is installed, use the Python base directory
    # and test if either Lib/module.py or site-packages/module.py exists.
    get_filename_component(_dirname "${SPHINX_BUILD_EXECUTABLE}" DIRECTORY)
    get_filename_component(_dirname "${_dirname}" DIRECTORY)
    if(IS_DIRECTORY "${_dirname}/Lib/${_ext}" OR
       IS_DIRECTORY "${_dirname}/site-packages/${_ext}")
      set(Sphinx_${_ext}_FOUND TRUE)
    else()
      set(Sphinx_${_ext}_FOUND FALSE)
    endif()
  endif()
endmacro()

#
# Find sphinx-build and sphinx-quickstart.
#
_Sphinx_find_executable(build)
_Sphinx_find_executable(quickstart)

#
# Verify both executables are part of the Sphinx distribution.
#
if(SPHINX_BUILD_EXECUTABLE AND SPHINX_QUICKSTART_EXECUTABLE)
  if(NOT SPHINX_BUILD_VERSION STREQUAL SPHINX_QUICKSTART_VERSION)
    message(FATAL_ERROR "Versions for sphinx-build (${SPHINX_BUILD_VERSION}) "
                        "and sphinx-quickstart (${SPHINX_QUICKSTART_VERSION}) "
                        "do not match")
  endif()
endif()

#
# To verify the required Sphinx extensions are available, the right Python
# installation must be queried (2 vs 3). Of course, this only makes sense on
# UNIX-like systems.
#
if(NOT CMAKE_HOST_WIN32 AND SPHINX_BUILD_EXECUTABLE)
  file(READ "${SPHINX_BUILD_EXECUTABLE}" _contents)
  if(_contents MATCHES "^#!([^\n]+)")
    string(STRIP "${CMAKE_MATCH_1}" _shebang)
    if(EXISTS "${_shebang}")
      set(_SPHINX_PYTHON_EXECUTABLE "${_shebang}")
    endif()
  endif()
endif()

foreach(_comp IN LISTS Sphinx_FIND_COMPONENTS)
  if(_comp STREQUAL "build")
    # Do nothing, sphinx-build is always required.
  elseif(_comp STREQUAL "quickstart")
    # Do nothing, sphinx-quickstart is optional, but looked up by default.
  elseif(_comp STREQUAL "breathe")
    _Sphinx_find_extension(${_comp})
  else()
    message(WARNING "${_comp} is not a valid or supported Sphinx extension")
    set(Sphinx_${_comp}_FOUND FALSE)
    continue()
  endif()
endforeach()

find_package_handle_standard_args(
  Sphinx
  VERSION_VAR SPHINX_BUILD_VERSION
  REQUIRED_VARS SPHINX_BUILD_EXECUTABLE SPHINX_BUILD_VERSION
  HANDLE_COMPONENTS)


# Generate a conf.py template file using sphinx-quickstart.
#
# sphinx-quickstart allows for quiet operation and a lot of settings can be
# specified as command line arguments, therefore its not required to parse the
# generated conf.py.
function(_Sphinx_generate_confpy _target _cachedir)
  if(NOT TARGET Sphinx::quickstart)
    message(FATAL_ERROR "sphinx-quickstart is not available, needed by"
                        "sphinx_add_docs for target ${_target}")
  endif()

  if(NOT DEFINED SPHINX_PROJECT)
    set(SPHINX_PROJECT ${PROJECT_NAME})
  endif()

  if(NOT DEFINED SPHINX_AUTHOR)
    set(SPHINX_AUTHOR "${SPHINX_PROJECT} committers")
  endif()

  if(NOT DEFINED SPHINX_COPYRIGHT)
    string(TIMESTAMP "%Y, ${SPHINX_AUTHOR}" SPHINX_COPYRIGHT)
  endif()

  if(NOT DEFINED SPHINX_VERSION)
    set(SPHINX_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
  endif()

  if(NOT DEFINED SPHINX_RELEASE)
    set(SPHINX_RELEASE "${PROJECT_VERSION}")
  endif()

  if(NOT DEFINED SPHINX_LANGUAGE)
    set(SPHINX_LANGUAGE "en")
  endif()

  if(NOT DEFINED SPHINX_MASTER)
    set(SPHINX_MASTER "index")
  endif()

  set(_known_exts autodoc doctest intersphinx todo coverage imgmath mathjax
                  ifconfig viewcode githubpages)

  if(DEFINED SPHINX_EXTENSIONS)
    foreach(_ext ${SPHINX_EXTENSIONS})
      set(_is_known_ext FALSE)
      foreach(_known_ext ${_known_exsts})
        if(_ext STREQUAL _known_ext)
          set(_opts "${opts} --ext-${_ext}")
          set(_is_known_ext TRUE)
          break()
        endif()
      endforeach()
      if(NOT _is_known_ext)
        if(_exts)
          set(_exts "${_exts},${_ext}")
        else()
          set(_exts "${_ext}")
        endif()
      endif()
    endforeach()
  endif()

  if(_exts)
    set(_exts "--extensions=${_exts}")
  endif()

  set(_templatedir "${CMAKE_CURRENT_BINARY_DIR}/${_target}.template")
  file(MAKE_DIRECTORY "${_templatedir}")
  execute_process(
    COMMAND "${SPHINX_QUICKSTART_EXECUTABLE}"
              -q --no-makefile --no-batchfile
              -p "${SPHINX_PROJECT}"
              -a "${SPHINX_AUTHOR}"
              -v "${SPHINX_VERSION}"
              -r "${SPHINX_RELEASE}"
              -l "${SPHINX_LANGUAGE}"
              --master "${SPHINX_MASTER}"
              ${_opts} ${_exts} "${_templatedir}"
    RESULT_VARIABLE _result
    OUTPUT_QUIET)

  if(_result EQUAL 0 AND EXISTS "${_templatedir}/conf.py")
    file(COPY "${_templatedir}/conf.py" DESTINATION "${_cachedir}")
  endif()

  file(REMOVE_RECURSE "${_templatedir}")

  if(NOT _result EQUAL 0 OR NOT EXISTS "${_cachedir}/conf.py")
    message(FATAL_ERROR "Sphinx configuration file not generated for "
                        "target ${_target}")
  endif()
endfunction()

function(sphinx_add_docs _target)
  set(_opts)
  set(_single_opts BUILDER OUTPUT_DIRECTORY SOURCE_DIRECTORY)
  set(_multi_opts BREATHE_PROJECTS)
  cmake_parse_arguments(_args "${_opts}" "${_single_opts}" "${_multi_opts}" ${ARGN})

  unset(SPHINX_BREATHE_PROJECTS)

  if(NOT _args_BUILDER)
    message(FATAL_ERROR "Sphinx builder not specified for target ${_target}")
  elseif(NOT _args_SOURCE_DIRECTORY)
    message(FATAL_ERROR "Sphinx source directory not specified for target ${_target}")
  else()
    if(NOT IS_ABSOLUTE "${_args_SOURCE_DIRECTORY}")
      get_filename_component(_sourcedir "${_args_SOURCE_DIRECTORY}" ABSOLUTE)
    else()
      set(_sourcedir "${_args_SOURCE_DIRECTORY}")
    endif()
    if(NOT IS_DIRECTORY "${_sourcedir}")
      message(FATAL_ERROR "Sphinx source directory '${_sourcedir}' for"
                          "target ${_target} does not exist")
    endif()
  endif()

  set(_builder "${_args_BUILDER}")
  if(_args_OUTPUT_DIRECTORY)
    set(_outputdir "${_args_OUTPUT_DIRECTORY}")
  else()
    set(_outputdir "${CMAKE_CURRENT_BINARY_DIR}/${_target}")
  endif()


  if(_args_BREATHE_PROJECTS)
    if(NOT Sphinx_breathe_FOUND)
      message(FATAL_ERROR "Sphinx extension 'breathe' is not available. Needed"
                          "by sphinx_add_docs for target ${_target}")
    endif()
    list(APPEND SPHINX_EXTENSIONS breathe)

    foreach(_doxygen_target ${_args_BREATHE_PROJECTS})
      if(TARGET ${_doxygen_target})
        list(APPEND _depends ${_doxygen_target})

        # Doxygen targets are supported. Verify that a Doxyfile exists.
        get_target_property(_dir ${_doxygen_target} BINARY_DIR)
        set(_doxyfile "${_dir}/Doxyfile.${_doxygen_target}")
        if(NOT EXISTS "${_doxyfile}")
          message(FATAL_ERROR "Target ${_doxygen_target} is not a Doxygen"
                              "target, needed by sphinx_add_docs for target"
                              "${_target}")
        endif()

        # Read the Doxyfile, verify XML generation is enabled and retrieve the
        # output directory.
        file(READ "${_doxyfile}" _contents)
        if(NOT _contents MATCHES "GENERATE_XML *= *YES")
          message(FATAL_ERROR "Doxygen target ${_doxygen_target} does not"
                              "generate XML, needed by sphinx_add_docs for"
                              "target ${_target}")
        elseif(_contents MATCHES "OUTPUT_DIRECTORY *= *([^ ][^\n]*)")
          string(STRIP "${CMAKE_MATCH_1}" _dir)
          set(_name "${_doxygen_target}")
          set(_dir "${_dir}/xml")
        else()
          message(FATAL_ERROR "Cannot parse Doxyfile generated by Doxygen"
                              "target ${_doxygen_target}, needed by"
                              "sphinx_add_docs for target ${_target}")
        endif()
      elseif(_doxygen_target MATCHES "([^: ]+) *: *(.*)")
        set(_name "${CMAKE_MATCH_1}")
        string(STRIP "${CMAKE_MATCH_2}" _dir)
      endif()

      if(_name AND _dir)
        if(_breathe_projects)
          set(_breathe_projects "${_breathe_projects}, \"${_name}\": \"${_dir}\"")
        else()
          set(_breathe_projects "\"${_name}\": \"${_dir}\"")
        endif()
        if(NOT _breathe_default_project)
          set(_breathe_default_project "${_name}")
        endif()
      endif()
    endforeach()
  endif()

  set(_cachedir "${CMAKE_CURRENT_BINARY_DIR}/${_target}.cache")
  file(MAKE_DIRECTORY "${_cachedir}")
  file(MAKE_DIRECTORY "${_cachedir}/_static")

  _Sphinx_generate_confpy(${_target} "${_cachedir}")

  if(_breathe_projects)
    file(APPEND "${_cachedir}/conf.py"
      "\nbreathe_projects = { ${_breathe_projects} }"
      "\nbreathe_default_project = '${_breathe_default_project}'")
  endif()

  add_custom_target(
    ${_target} ALL
    COMMAND ${SPHINX_BUILD_EXECUTABLE}
              -b ${_builder}
              -d "${CMAKE_CURRENT_BINARY_DIR}/${_target}.cache/_doctrees"
              -c "${CMAKE_CURRENT_BINARY_DIR}/${_target}.cache"
              "${_sourcedir}"
              "${_outputdir}"
    DEPENDS ${_depends})
endfunction()

