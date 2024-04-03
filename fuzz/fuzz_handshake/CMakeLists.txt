project(fuzz_handshake LANGUAGES CXX)
cmake_minimum_required(VERSION 3.9)

if(NOT TARGET CycloneDDS::ddsc)
  # Find the CycloneDDS package.
  find_package(CycloneDDS REQUIRED)
endif()

# Protobuf in newer versions depend on absl (and utf8_range). These
# dependencies are not yet tracked by every cmake distribution. This makes it
# necessary to source the cmake configuration files from the build directories
# of protobuf (using the CONFIG argument to find_package)...
set(PROTOBUF_ROOT "${CMAKE_BINARY_DIR}/../LPM/external.protobuf")

set(absl_DIR "${PROTOBUF_ROOT}/lib/cmake/absl")
find_package(absl REQUIRED CONFIG)

set(utf8_range_DIR "${PROTOBUF_ROOT}/lib/cmake/utf8_range")
find_package(utf8_range REQUIRED CONFIG)

set(protobuf_DIR "${PROTOBUF_ROOT}/lib/cmake/protobuf")
# ...sometime in the future these two statements should suffice:
set(Protobuf_USE_STATIC_LIBS ON)
find_package(protobuf REQUIRED CONFIG)

add_library(fuzz_handshake_harness fuzz_handshake_harness.c)
target_include_directories(
  fuzz_handshake_harness PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/ddsrt/include>"
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/core/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsc/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsc/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsi/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsi/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/cdr/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/ddsrt/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/security/core/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/security/core/tests>"
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/security/core/tests>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/security/api/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../install/include>")

add_executable(fuzz_handshake fuzz_handshake.cc)
protobuf_generate(TARGET fuzz_handshake LANGUAGE cpp PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/fuzz_handshake.proto")
target_include_directories(
  fuzz_handshake PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/ddsrt/include>"
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/../libprotobuf-mutator>"
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/../LPM/external.protobuf/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../install/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/>")


find_package(OpenSSL REQUIRED)
target_link_libraries(fuzz_handshake
    fuzz_handshake_harness
    ${CMAKE_BINARY_DIR}/../LPM/src/libfuzzer/libprotobuf-mutator-libfuzzer.a
    ${CMAKE_BINARY_DIR}/../LPM/src/libprotobuf-mutator.a
    protobuf::libprotobuf
    CycloneDDS::ddsc
    $ENV{LIB_FUZZING_ENGINE}
    OpenSSL::SSL
    stdc++)
