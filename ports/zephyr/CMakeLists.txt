cmake_minimum_required(VERSION 3.25.0)

# Uncomment for (gdbserver) debugging
if(BOARD MATCHES "qemu_.*")
  list(APPEND QEMU_EXTRA_FLAGS -serial tcp:127.0.0.1:5678,server)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dds_examples)

include(ExternalProject)
include(CMakePrintHelpers)

set(BUILD_DDSPERF_PING 0 CACHE BOOL "Build the CycloneDDS DDSPerf tool (ping mode)")
set(BUILD_DDSPERF_PONG 0 CACHE BOOL "Build the CycloneDDS DDSPerf tool (pong mode)")
set(BUILD_ROUNDTRIP_PING 0 CACHE BOOL "Build the CycloneDDS Roundtrip Ping example")
set(BUILD_ROUNDTRIP_PONG 0 CACHE BOOL "Build the CycloneDDS Roundtrip Pong example")
set(BUILD_THROUGHPUT_PUB 0 CACHE BOOL "Build the CycloneDDS Throughput Publisher example")
set(BUILD_THROUGHPUT_SUB 0 CACHE BOOL "Build the CycloneDDS Throughput Subscriber example")
set(BUILD_HELLOWORLD_PUB 0 CACHE BOOL "Build the CycloneDDS Hello World Publisher example")
set(BUILD_HELLOWORLD_SUB 0 CACHE BOOL "Build the CycloneDDS Hello World Subscriber example")

function(GENERATE_CDDS_CONF XML_FILE OUTPUT_FILE)
  message("Generate static CycloneDDS config (${XML_FILE} -> ${OUTPUT_FILE})")

  # Strip one-line comments, newlines, whitespace
  #file(READ ${XML_FILE} xmlout)
  #string(REGEX REPLACE "<!--[^\r\n]*-->" "" xmlout ${xmlout})
  #string(REGEX REPLACE "(\r?\n)+" "" xmlout ${xmlout})
  #string(REGEX REPLACE ">[ ]+<" "" xmlout ${xmlout})

  find_program(xxd xxd REQUIRED)
  execute_process(
    COMMAND ${xxd} -i
    INPUT_FILE ${XML_FILE}
    OUTPUT_VARIABLE conf_data
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)

  # Convert the string "CYCLONEDDS_URI=" to C array of bytes
  string(HEX "CYCLONEDDS_URI=" conf_prefix)
  string(REGEX MATCHALL "([A-Fa-f0-9][A-Fa-f0-9])" conf_prefix ${conf_prefix})
  list(JOIN conf_prefix ", 0x" conf_prefix)
  string(PREPEND conf_prefix "  0x")

  # Write to output file
  file(WRITE ${OUTPUT_FILE}
    "unsigned char cdds_xml_config[] = {\n"
    ${conf_prefix} ",\n"
    ${conf_data} ", 0\n"
    " };\n")
endfunction()

zephyr_get_system_include_directories_for_lang_as_string(C ext_system_includes)
zephyr_get_include_directories_for_lang_as_string(C ext_includes)
zephyr_get_compile_definitions_for_lang_as_string(C ext_defs)
zephyr_get_compile_options_for_lang_as_String(C ext_opts)
set(CDDS_CFLAGS
    "${ext_defs} -D_POSIX_C_SOURCE=200809L ${ext_system_includes} ${ext_includes} ${ext_opts}"
)

set(CDDS_LIB_DIR ${CMAKE_CURRENT_BINARY_DIR}/cyclonedds-prefix/lib)
set(CDDS_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/cyclonedds-prefix/include)

ExternalProject_Add(cyclonedds
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../
  BUILD_COMMAND ${CMAKE_COMMAND} --build .
  CMAKE_ARGS
  -DBUILD_SHARED_LIBS=0 -DENABLE_SECURITY=0 -DENABLE_SSL=0 -DENABLE_SOURCE_SPECIFIC_MULTICAST=0 -DENABLE_IPV6=1 -DENABLE_SHM=0 -DWITH_ZEPHYR=1 
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DCMAKE_VERBOSE_MAKEFILE=1
    -DCMAKE_C_FLAGS=${CDDS_CFLAGS}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_SYSTEM_NAME=Generic
    -DCMAKE_BUILD_TYPE=Debug
  DEPENDS zephyr_interface
  BUILD_BYPRODUCTS ${CDDS_LIB_DIR}/libddsc.a
)
add_library(cdds_lib STATIC IMPORTED GLOBAL)
add_dependencies(cdds_lib cyclonedds)
set_target_properties(cdds_lib PROPERTIES IMPORTED_LOCATION ${CDDS_LIB_DIR}/libddsc.a)

target_include_directories(app PUBLIC ${CDDS_INCLUDE_DIR})
target_link_libraries(app PUBLIC cdds_lib)

if (BUILD_DDSPERF_PING OR BUILD_DDSPERF_PONG)
  GENERATE_CDDS_CONF("config.xml" "${CDDS_INCLUDE_DIR}/ddsperf_config.h")
  target_compile_definitions(app
    PRIVATE
    BUILD_DDSPERF_PING=${BUILD_DDSPERF_PING}
    BUILD_DDSPERF_PONG=${BUILD_DDSPERF_PONG})
  target_sources(app
    PRIVATE
    src/ddsperf_main.c
    src/ddsperf.c
    src/ddsperf_types.c src/ddsperf_types.h
    src/cputime.c src/cputime.h
    src/netload.c src/netload.h
    src/async_listener.c src/async_listener.h
  )
elseif(BUILD_ROUNDTRIP_PING OR BUILD_ROUNDTRIP_PONG)
  GENERATE_CDDS_CONF("config.xml" "${CDDS_INCLUDE_DIR}/roundtrip_config.h")
  target_compile_definitions(app
    PRIVATE
    BUILD_ROUNDTRIP_PING=${BUILD_ROUNDTRIP_PING}
    BUILD_ROUNDTRIP_PONG=${BUILD_ROUNDTRIP_PONG})
  target_sources(app
    PRIVATE
    src/roundtrip_main.c
    src/RoundTrip.c src/RoundTrip.h
    src/ping.c src/pong.c)
elseif(BUILD_THROUGHPUT_PUB OR BUILD_THROUGHPUT_SUB)
  GENERATE_CDDS_CONF("config.xml" "${CDDS_INCLUDE_DIR}/throughput_config.h")
  target_compile_definitions(app
    PRIVATE
    BUILD_THROUGHPUT_PUB=${BUILD_THROUGHPUT_PUB}
    BUILD_THROUGHPUT_SUB=${BUILD_THROUGHPUT_SUB})
  target_sources(app
    PRIVATE
    src/throughput_main.c
    src/Throughput.c src/Throughput.h
    src/subscriber.c src/publisher.c)

elseif(BUILD_HELLOWORLD_PUB OR BUILD_HELLOWORLD_SUB)
  target_compile_definitions(app
    PRIVATE
    BUILD_HELLOWORLD_PUB=${BUILD_HELLOWORLD_PUB}
    BUILD_HELLOWORLD_SUB=${BUILD_HELLOWORLD_SUB})
  target_sources(app
    PRIVATE
    src/main.c
    src/HelloWorldData.c
    src/HelloWorldData.h
  )
endif()
