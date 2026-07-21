project(fuzz_sample_deser LANGUAGES C CXX)
cmake_minimum_required(VERSION 3.16)

if(NOT TARGET CycloneDDS::ddsc)
  # Find the CycloneDDS package.
  find_package(CycloneDDS REQUIRED)
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(FUZZ_SAMPLE_DESER_SEED "" CACHE STRING "Seed for generated fuzz_sample_deser IDL")
if(NOT FUZZ_SAMPLE_DESER_SEED)
  execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE FUZZ_SAMPLE_DESER_SEED
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
endif()
if(NOT FUZZ_SAMPLE_DESER_SEED MATCHES "^[0-9a-fA-F]+$")
  message(FATAL_ERROR "FUZZ_SAMPLE_DESER_SEED must be a hexadecimal seed")
endif()

set(fuzz_sample_deser_include_dirs
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsc/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/cdr/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsi/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/ddsi/src>")

function(add_fuzz_sample_deser_variant target suffix xcdr_version corpus_name)
  set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/${suffix}")
  set(generated_idl "${generated_dir}/fuzz_sample.idl")
  set(generated_samples "${generated_dir}/fuzz_samples.h")
  set(idlc_target "fuzz_sample_${suffix}")

  add_custom_command(
    OUTPUT "${generated_idl}" "${generated_samples}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${generated_dir}"
    COMMAND "${CMAKE_COMMAND}" -E env
            "PATH=$ENV{PATH}"
            "PYTHONPATH=$ENV{PYTHONPATH}"
            "CYCLONEDDS_HOME=$ENV{CYCLONEDDS_HOME}"
            "LD_LIBRARY_PATH=$ENV{LD_LIBRARY_PATH}"
            "DYLD_LIBRARY_PATH=$ENV{DYLD_LIBRARY_PATH}"
            "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/generate_idl.py"
            --xcdr-version "${xcdr_version}"
            --corpus-name "${corpus_name}"
            "${FUZZ_SAMPLE_DESER_SEED}"
            "${generated_dir}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/generate_idl.py"
    COMMENT "Generating ${target} IDL"
    VERBATIM)

  idlc_generate(
    TARGET "${idlc_target}"
    FILES "${generated_idl}"
    OUTPUT_DIR "${generated_dir}"
    WARNINGS no-implicit-extensibility)

  set_source_files_properties("${generated_samples}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
  add_executable("${target}" fuzz_sample_deser.c "${generated_samples}")
  target_compile_definitions(
    "${target}" PRIVATE
    "FUZZ_SAMPLE_DESER_XCDR_VERSION=DDSI_RTPS_CDR_ENC_VERSION_${xcdr_version}")
  target_include_directories(
    "${target}" PRIVATE
    "${generated_dir}"
    ${fuzz_sample_deser_include_dirs})
  set_target_properties("${target}" PROPERTIES LINKER_LANGUAGE CXX)
  target_link_libraries("${target}" "${idlc_target}" CycloneDDS::ddsc $ENV{LIB_FUZZING_ENGINE})
endfunction()

add_fuzz_sample_deser_variant(
  fuzz_sample_deser_xcdr1 xcdr1 1 fuzz_sample_deser_xcdr1_seed_corpus)

# Keep the historical OSS-Fuzz target name for the already well-fuzzed XCDR2 path.
add_fuzz_sample_deser_variant(
  fuzz_sample_deser xcdr2 2 fuzz_sample_deser_seed_corpus)
