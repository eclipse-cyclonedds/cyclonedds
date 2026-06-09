project(fuzz_type_graph LANGUAGES CXX)
cmake_minimum_required(VERSION 3.16)

if(NOT TARGET CycloneDDS::ddsc)
  # Find the CycloneDDS package.
  find_package(CycloneDDS REQUIRED)
endif()

# This follows fuzz_handshake: local.sh and oss-fuzz prepare LPM next to the
# build directory.
set(PROTOBUF_ROOT "${CMAKE_BINARY_DIR}/../LPM/external.protobuf")

set(absl_DIR "${PROTOBUF_ROOT}/lib/cmake/absl")
find_package(absl REQUIRED CONFIG)

set(utf8_range_DIR "${PROTOBUF_ROOT}/lib/cmake/utf8_range")
find_package(utf8_range REQUIRED CONFIG)

set(protobuf_DIR "${PROTOBUF_ROOT}/lib/cmake/protobuf")
set(Protobuf_USE_STATIC_LIBS ON)
find_package(protobuf REQUIRED CONFIG)

add_executable(fuzz_type_graph fuzz_type_graph.cc)
protobuf_generate(TARGET fuzz_type_graph LANGUAGE cpp PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/fuzz_type_graph.proto")
target_include_directories(
  fuzz_type_graph PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/ddsrt/include>"
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/../libprotobuf-mutator>"
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/../LPM/external.protobuf/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsc/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsc/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/cdr/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsi/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsi/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/ddsrt/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")

target_link_libraries(fuzz_type_graph
    ${CMAKE_BINARY_DIR}/../LPM/src/libfuzzer/libprotobuf-mutator-libfuzzer.a
    ${CMAKE_BINARY_DIR}/../LPM/src/libprotobuf-mutator.a
    protobuf::libprotobuf
    CycloneDDS::ddsc
    $ENV{LIB_FUZZING_ENGINE}
    stdc++)
