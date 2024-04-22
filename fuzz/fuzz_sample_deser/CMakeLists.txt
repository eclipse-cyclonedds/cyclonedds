cmake_minimum_required(VERSION 3.5)
project(fuzz_sample_deser LANGUAGES C)

if(NOT TARGET CycloneDDS::ddsc)
  # Find the CycloneDDS package.
  find_package(CycloneDDS REQUIRED)
endif()

idlc_generate(TARGET fuzz_sample FILES fuzz_sample.idl WARNINGS no-implicit-extensibility)
add_executable(fuzz_sample_deser fuzz_sample_deser.c fuzz_sample.c)
target_include_directories(
  fuzz_sample_deser PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsc/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/cdr/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsi/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsi/src>")
  
set_target_properties(fuzz_sample_deser PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(fuzz_sample_deser CycloneDDS::ddsc $ENV{LIB_FUZZING_ENGINE})
