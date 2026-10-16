# Copyright(c) 2026 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

cmake_minimum_required(VERSION 3.20.0)

# Use the Cyclone DDS checkout this sample lives in as Zephyr module, so the
# sample also builds when Cyclone DDS is not part of the west manifest.
cmake_path(SET cyclonedds_dir NORMALIZE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
list(APPEND ZEPHYR_EXTRA_MODULES ${cyclonedds_dir})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cyclonedds_samples)

# Without any of these the self-test in src/main.c is built.
set(BUILD_DDSPERF_PING 0 CACHE BOOL "Build the CycloneDDS DDSPerf tool (ping mode)")
set(BUILD_DDSPERF_PONG 0 CACHE BOOL "Build the CycloneDDS DDSPerf tool (pong mode)")
set(BUILD_ROUNDTRIP_PING 0 CACHE BOOL "Build the CycloneDDS Roundtrip Ping example")
set(BUILD_ROUNDTRIP_PONG 0 CACHE BOOL "Build the CycloneDDS Roundtrip Pong example")
set(BUILD_THROUGHPUT_PUB 0 CACHE BOOL "Build the CycloneDDS Throughput Publisher example")
set(BUILD_THROUGHPUT_SUB 0 CACHE BOOL "Build the CycloneDDS Throughput Subscriber example")
set(BUILD_HELLOWORLD_PUB 0 CACHE BOOL "Build the CycloneDDS Hello World Publisher example")
set(BUILD_HELLOWORLD_SUB 0 CACHE BOOL "Build the CycloneDDS Hello World Subscriber example")

# Converts an XML configuration file into a "CYCLONEDDS_URI=<xml>" C array.
function(generate_cdds_conf XML_FILE OUTPUT_FILE)
  find_program(xxd xxd REQUIRED)
  execute_process(
    COMMAND ${xxd} -i
    INPUT_FILE ${XML_FILE}
    OUTPUT_VARIABLE conf_data
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
  string(HEX "CYCLONEDDS_URI=" conf_prefix)
  string(REGEX MATCHALL "([A-Fa-f0-9][A-Fa-f0-9])" conf_prefix ${conf_prefix})
  list(JOIN conf_prefix ", 0x" conf_prefix)
  string(PREPEND conf_prefix "  0x")
  file(WRITE ${OUTPUT_FILE}
    "unsigned char cdds_xml_config[] = {\n"
    ${conf_prefix} ",\n"
    ${conf_data} ", 0\n"
    " };\n")
endfunction()

set(generated_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_include_directories(app PRIVATE ${generated_dir})

if(BUILD_DDSPERF_PING OR BUILD_DDSPERF_PONG)
  generate_cdds_conf("config.xml" "${generated_dir}/ddsperf_config.h")
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
  generate_cdds_conf("config.xml" "${generated_dir}/roundtrip_config.h")
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
  generate_cdds_conf("config.xml" "${generated_dir}/throughput_config.h")
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
    src/helloworld_main.c
    src/HelloWorldData.c
    src/HelloWorldData.h
  )
else()
  target_sources(app PRIVATE src/main.c)
endif()
