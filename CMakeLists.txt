#
# Copyright(c) 2006 to 2022 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
cmake_minimum_required(VERSION 3.16)
project(CycloneDDS VERSION 0.11.0 LANGUAGES C)

# Set a default build type if none was specified
set(default_build_type "RelWithDebInfo")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(CMAKE_CROSSCOMPILING)
  set(not_crosscompiling OFF)
else()
  set(not_crosscompiling ON)
endif()

# By default don't treat warnings as errors, else anyone building it with a different compiler that
# just happens to generate a warning, as well as anyone adding or modifying something and making a
# small mistake would run into errors.  CI builds can be configured differently.
option(WERROR "Treat compiler warnings as errors" OFF)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/Modules")

option(BUILD_IDLC "Build IDL preprocessor" ${not_crosscompiling})
option(BUILD_DDSPERF "Build ddsperf tool" ${not_crosscompiling})

option(WITH_ZEPHYR "Build for Zephyr RTOS" OFF)

set(CMAKE_C_STANDARD 99)
if(CMAKE_SYSTEM_NAME STREQUAL "VxWorks")
  add_definitions(-std=c99)
endif()

if(${CMAKE_C_COMPILER_ID} STREQUAL "SunPro")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64 -xc99 -D__restrict=restrict -D__deprecated__=")
  set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -m64")
elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
  set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -m64")
endif()

# Homebrew
if(APPLE)
  # Hint Homebrew prefixes to find_* commands. Use of CMAKE_PREFIX_PATH is
  # required because full signature mode for find_package implies Config mode.
  find_program(brew NAMES brew)
  if (brew)
    foreach(formula bison)
      execute_process(
        COMMAND ${brew} --prefix ${formula}
        OUTPUT_VARIABLE prefix
        ERROR_QUIET)
      string(STRIP "${prefix}" prefix)
      if(EXISTS "${prefix}")
        # Append to ensure user specified paths come first.
        set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${prefix}")
      endif()
    endforeach()
  endif()
endif()

# Conan
if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
  include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
  if(APPLE)
    # By default Conan strips all RPATHs (see conanbuildinfo.cmake), which
    # causes tests to fail as the executables cannot find the library target.
    # By setting KEEP_RPATHS, Conan does not set CMAKE_SKIP_RPATH and the
    # resulting binaries still have the RPATH information. This is fine because
    # CMake will strip the build RPATH information in the install step.
    #
    # NOTE:
    # Conan's default approach is to use the "imports" feature, which copies
    # all the dependencies into the bin directory. Of course, this doesn't work
    # quite that well for libraries generated in this Project (see Conan
    # documentation).
    #
    # See the links below for more information.
    # https://github.com/conan-io/conan/issues/337
    # https://docs.conan.io/en/latest/howtos/manage_shared_libraries/rpaths.html
    # https://gitlab.kitware.com/cmake/community/wikis/doc/cmake/RPATH-handling
    conan_basic_setup(KEEP_RPATHS)
  else()
    conan_basic_setup()
  endif()
  conan_define_targets()
endif()

# Set reasonably strict warning options for clang, gcc, msvc
# Enable coloured ouput if Ninja is used for building
if("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" OR
   "${CMAKE_C_COMPILER_ID}" STREQUAL "AppleClang")
  #message(STATUS clang)
  set(wflags "-Wall"
             "-Wextra"
             "-Wconversion"
             "-Wunused"
             "-Wmissing-prototypes"
             "-Winfinite-recursion"
             "-Wassign-enum"
             "-Wcomma"
             "-Wmissing-prototypes"
             "-Wdocumentation"
             "-Wstrict-prototypes"
             "-Wconditional-uninitialized"
             "-Wshadow")
  add_compile_options(${wflags})
  add_link_options(-undefined error)
  if(${WERROR})
    add_compile_options(-Werror)
  endif()
  if("${CMAKE_GENERATOR}" STREQUAL "Ninja")
    add_compile_options(-Xclang -fcolor-diagnostics)
  endif()
elseif("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
  #message(STATUS gcc)
  if(WITH_ZEPHYR)
    # Suppress false warnings from system includes
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wmissing-prototypes)
  else()
    add_compile_options(-Wall -Wextra -Wconversion -Wmissing-prototypes)
  endif()
  if(${WERROR})
    add_compile_options(-Werror)
  endif()
  if("${CMAKE_GENERATOR}" STREQUAL "Ninja")
    add_compile_options(-fdiagnostics-color=always)
  endif()
elseif("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
  #message(STATUS msvc)
  add_compile_options(/W3)
  if(${WERROR})
    add_compile_options(/WX)
  endif()
endif()

# I don't know how to enable warnings properly so that they are enabled in Xcode projects as well
if(${CMAKE_GENERATOR} STREQUAL "Xcode")
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_EMPTY_BODY YES)
  set (CMAKE_XCODE_ATTRIBUTE_GCC_WARN_SHADOW YES)
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_BOOL_CONVERSION YES)
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_CONSTANT_CONVERSION YES)
  set (CMAKE_XCODE_ATTRIBUTE_GCC_WARN_64_TO_32_BIT_CONVERSION YES)
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_ENUM_CONVERSION YES)
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_FLOAT_CONVERSION YES)
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_INT_CONVERSION YES)
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_NON_LITERAL_NULL_CONVERSION YES)
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_IMPLICIT_SIGN_CONVERSION YES)
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_INFINITE_RECURSION YES)
  set (CMAKE_XCODE_ATTRIBUTE_GCC_WARN_INITIALIZER_NOT_FULLY_BRACKETED YES)
  set (CMAKE_XCODE_ATTRIBUTE_GCC_WARN_ABOUT_RETURN_TYPE YES)
  set (CMAKE_XCODE_ATTRIBUTE_GCC_WARN_MISSING_PARENTHESES YES)
  set (CMAKE_XCODE_ATTRIBUTE_GCC_WARN_ABOUT_MISSING_FIELD_INITIALIZERS YES)
  set (CMAKE_XCODE_ATTRIBUTE_GCC_WARN_ABOUT_MISSING_NEWLINE YES)
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_ASSIGN_ENUM YES)
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_SEMICOLON_BEFORE_METHOD_BODY YES)
  set (CMAKE_XCODE_ATTRIBUTE_GCC_WARN_SIGN_COMPARE YES)
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_STRICT_PROTOTYPES YES)
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_COMMA YES)
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_SUSPICIOUS_IMPLICIT_CONVERSION YES)
  set (CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNINITIALIZED_AUTOS YES_AGGRESSIVE)
  set (CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_FUNCTION YES)
  set (CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_LABEL YES)
  set (CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_PARAMETER YES)
  set (CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_VALUE YES)
  set (CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_VARIABLE YES)
  set (CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_DOCUMENTATION_COMMENTS YES)
  set (CMAKE_XCODE_ATTRIBUTE_GCC_WARN_ABOUT_MISSING_PROTOTYPES YES)
endif()

# Make it easy to enable MSVC, Clang's/gcc's analyzers
set(ANALYZER "" CACHE STRING "Analyzer to enable on the build.")
if(ANALYZER)
  # GCC and Visual Studio offer builtin analyzers. Clang supports static
  # analysis through separate tools, e.g. Clang-Tidy, which can be used in
  # conjunction with other compilers too. Specifying -DANALYZER=on enables
  # the builtin analyzer for the compiler, enabling clang-tidy in case of
  # Clang. Specifying -DANALYZER=clang-tidy always enables clang-tidy.
  string(REPLACE " " "" ANALYZER "${ANALYZER}")
  string(TOLOWER "${ANALYZER}" ANALYZER)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND ANALYZER STREQUAL "on")
    set(ANALYZER "clang-tidy")
  endif()

  if(ANALYZER STREQUAL "clang-tidy")
    # Clang-Tidy is an extensible tool that offers more than static analysis.
    # https://clang.llvm.org/extra/clang-tidy/checks/list.html
    message(STATUS "Enabling analyzer: clang-tidy")
    set(CMAKE_C_CLANG_TIDY "clang-tidy;-checks=-*,clang-analyzer-*,-clang-analyzer-security.insecureAPI.strcpy")
    if(WERROR)
      set(CMAKE_C_CLANG_TIDY "${CMAKE_C_CLANG_TIDY};--warnings-as-errors=*")
    endif()
  elseif(ANALYZER STREQUAL "on")
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
      # GCC 10 static analyzer is a bit problematic
      # GCC 11 is a full rewrite, also not quite happy with the code
      # GCC 12 is pretty much happy, but some parts of IDLC (main file, type object
      # generation) still cause problems, but we can disable those
      if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL "12")
        message(STATUS "Enabling analyzer: GCC")
        add_compile_options(-fanalyzer)
      endif()
    endif()
  endif()
endif()
set(SANITIZER "" CACHE STRING "Sanitizers to enable on the build.")
if(SANITIZER)
  string(REGEX REPLACE " " "" SANITIZER "${SANITIZER}")
  string(REGEX REPLACE "[,;]+" ";" SANITIZER "${SANITIZER}")
  foreach(san ${SANITIZER})
    if(san STREQUAL "address")
      add_compile_options("-fno-omit-frame-pointer")
      add_link_options("-fno-omit-frame-pointer")
    endif()
    if(san AND NOT san STREQUAL "none")
      message(STATUS "Enabling sanitizer: ${san}")
      add_compile_options("-fsanitize=${san}")
      add_link_options("-fsanitize=${san}")
    endif()
  endforeach()
endif()

find_package(codecov)
set(MEMORYCHECK_COMMAND_OPTIONS "--track-origins=yes --leak-check=full --trace-children=yes --child-silent-after-fork=yes --xml=yes --xml-file=TestResultValgrind_%p.xml --tool=memcheck --show-reachable=yes --leak-resolution=high")

# By default building the testing tree is enabled by including CTest, but
# since not everybody has CUnit, and because it is not strictly required to
# build the product itself, switch to off by default.
option(BUILD_TESTING "Build the testing tree." OFF)

# Include the xtests for idlc. These tests use the idl compiler (C back-end) to
# compile an idl file at (test) runtime, and use the C compiler to build a test
# application for the generated types, that is executed to do the actual testing.
option(BUILD_IDLC_XTESTS "Build the idlc compile tests" OFF)

# Disable building examples by default because it is not strictly required.
option(BUILD_EXAMPLES "Build examples." OFF)

option(ENABLE_LTO "Enable link time optimization." ON)

if(MSVC)
  option(INSTALL_PDB "Install PDB files next to DLLs." ${BUILD_SHARED_LIBS})
endif()

# Build all executables and libraries into the top-level /bin and /lib folders.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(CTest)

option(APPEND_PROJECT_NAME_TO_INCLUDEDIR
  "When ON headers are installed to a folder ending with an extra ${PROJECT_NAME}. \
  This avoids include directory search order issues when overriding this package
  from a merged catkin, ament, or colcon workspace." OFF)

if(APPEND_PROJECT_NAME_TO_INCLUDEDIR)
  set(CMAKE_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}")
endif()

if(APPLE)
  set(CMAKE_INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
else()
  set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

if(MINGW)
  # Require at least Windows 7
  add_definitions(-D_WIN32_WINNT=_WIN32_WINNT_WIN7)
  add_definitions(-DNTDDI_VERSION=NTDDI_WIN7)
  add_definitions(-D__USE_MINGW_ANSI_STDIO=1) # prefer C99 conformance
  # Do not prefix libraries with "lib"
  set(CMAKE_SHARED_LIBRARY_PREFIX "")
  set(CMAKE_STATIC_LIBRARY_PREFIX "")
endif()

set(CMAKE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

# Generate <Package>Config.cmake
configure_package_config_file(
  "PackageConfig.cmake.in"
  "${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_CMAKEDIR}")

# Generate <Package>Version.cmake
write_basic_package_version_file(
  "${PROJECT_NAME}ConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_CMAKEDIR}"
  COMPONENT dev)

# Generate <Package>Targets.cmake
install(
  EXPORT ${PROJECT_NAME}
  FILE "${PROJECT_NAME}Targets.cmake"
  NAMESPACE "${PROJECT_NAME}::"
  DESTINATION "${CMAKE_INSTALL_CMAKEDIR}"
  COMPONENT dev)

# Generate Pkg-Config file
if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
  set(CYCLONEDDS_PKG_CONFIG_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
else()
  set(CYCLONEDDS_PKG_CONFIG_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
  set(CYCLONEDDS_PKG_CONFIG_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
else()
  set(CYCLONEDDS_PKG_CONFIG_LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()
configure_file(
  "PkgConfig.pc.in"
  "${PROJECT_NAME}.pc"
  @ONLY
)
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
  COMPONENT dev
)

# Generated file paths
# although the following files are generated, they are checked into source
# control for convenience, but the user must copy them manually
set(generated_cyclonedds_rnc "${CMAKE_CURRENT_SOURCE_DIR}/etc/cyclonedds.rnc")
set(generated_cyclonedds_xsd "${CMAKE_CURRENT_SOURCE_DIR}/etc/cyclonedds.xsd")
set(generated_options_md     "${CMAKE_CURRENT_SOURCE_DIR}/docs/manual/options.md")
set(generated_options_rst    "${CMAKE_CURRENT_SOURCE_DIR}/docs/manual/config/config_file_reference.rst")
set(generated_defconfig_src  "${CMAKE_CURRENT_SOURCE_DIR}/src/core/ddsi/defconfig.c")

add_subdirectory(compat)
add_subdirectory(src)
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(DEFINED ENV{LIB_FUZZING_ENGINE})
  add_subdirectory(fuzz)
endif()

include(CMakeCPack.cmake)
