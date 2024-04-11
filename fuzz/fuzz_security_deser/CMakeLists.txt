project(fuzz_security_deser LANGUAGES C)
cmake_minimum_required(VERSION 3.5)

if(NOT TARGET CycloneDDS::ddsc)
  # Find the CycloneDDS package.
  find_package(CycloneDDS REQUIRED)
endif()

add_executable(fuzz_security_deser fuzz_security_deser.c)
target_include_directories(
  fuzz_security_deser PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsi/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/security/core/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/security/api/include>")
set_target_properties(fuzz_security_deser PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(fuzz_security_deser CycloneDDS::ddsc $ENV{LIB_FUZZING_ENGINE})
