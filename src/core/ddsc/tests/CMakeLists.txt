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
include(CUnit)

idlc_generate(TARGET RoundTrip FILES RoundTrip.idl)
idlc_generate(TARGET Space FILES Space.idl WARNINGS no-implicit-extensibility)
idlc_generate(TARGET TypesArrayKey FILES TypesArrayKey.idl WARNINGS no-implicit-extensibility)
idlc_generate(TARGET WriteTypes FILES WriteTypes.idl WARNINGS no-implicit-extensibility)
idlc_generate(TARGET InstanceHandleTypes FILES InstanceHandleTypes.idl WARNINGS no-implicit-extensibility)
idlc_generate(TARGET RWData FILES RWData.idl WARNINGS no-implicit-extensibility)
idlc_generate(TARGET CreateWriter FILES CreateWriter.idl WARNINGS no-implicit-extensibility)
idlc_generate(TARGET DataRepresentationTypes FILES DataRepresentationTypes.idl WARNINGS no-implicit-extensibility)
idlc_generate(TARGET MinXcdrVersion FILES MinXcdrVersion.idl)
idlc_generate(TARGET CdrStreamOptimize FILES CdrStreamOptimize.idl WARNINGS no-implicit-extensibility)
idlc_generate(TARGET CdrStreamSkipDefault FILES CdrStreamSkipDefault.idl)
if(ENABLE_TYPE_DISCOVERY)
  idlc_generate(TARGET XSpace FILES XSpace.idl XSpaceEnum.idl XSpaceMustUnderstand.idl XSpaceTypeConsistencyEnforcement.idl WARNINGS no-implicit-extensibility no-inherit-appendable)
  idlc_generate(TARGET XSpaceNoTypeInfo FILES XSpaceNoTypeInfo.idl NO_TYPE_INFO WARNINGS no-implicit-extensibility)
  idlc_generate(TARGET TypeBuilderTypes FILES TypeBuilderTypes.idl WARNINGS no-implicit-extensibility)
endif()

set(ddsc_test_sources
    "asymdisconnect.c"
    "basic.c"
    "builtin_topics.c"
    "cdr.c"
    "config.c"
    "data_avail_stress.c"
    "destorder.c"
    "discstress.c"
    "dispose.c"
    "domain.c"
    "domain_torture.c"
    "entity_api.c"
    "entity_hierarchy.c"
    "entity_status.c"
    "err.c"
    "filter.c"
    "instance_get_key.c"
    "instance_handle.c"
    "listener.c"
    "liveliness.c"
    "loan.c"
    "multi_sertype.c"
    "nwpart.c"
    "participant.c"
    "pp_lease_dur.c"
    "publisher.c"
    "qos.c"
    "qosmatch.c"
    "querycondition.c"
    "guardcondition.c"
    "readcondition.c"
    "reader.c"
    "reader_iterator.c"
    "read_instance.c"
    "register.c"
    "subscriber.c"
    "take_instance.c"
    "time.c"
    "time_based_filter.c"
    "topic.c"
    "topic_find_local.c"
    "transientlocal.c"
    "types.c"
    "uninitialized.c"
    "unregister.c"
    "unsupported.c"
    "userdata.c"
    "waitset.c"
    "waitset_torture.c"
    "whc.c"
    "write.c"
    "write_various_types.c"
    "writer.c"
    "test_util.c"
    "test_util.h"
    "test_common.h"
    "test_common.c"
    "test_oneliner.c"
    "test_oneliner.h"
    "cdrstream.c"
  )

if(ENABLE_LIFESPAN)
  list(APPEND ddsc_test_sources "lifespan.c")
endif()

if(ENABLE_DEADLINE_MISSED)
  list(APPEND ddsc_test_sources "deadline.c")
endif()

if(ENABLE_TYPE_DISCOVERY)
  list(APPEND ddsc_test_sources
    "xtypes.c"
    "data_representation.c"
    "typebuilder.c"
    "dynamic_type.c"
  )
endif()

if(ENABLE_TOPIC_DISCOVERY)
  list(APPEND ddsc_test_sources
    "typelookup.c"
    "topic_discovery.c"
    "topic_find_global.c")
endif()

add_cunit_executable(cunit_ddsc ${ddsc_test_sources})
target_include_directories(
  cunit_ddsc PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/include/>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsc/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../cdr/include>")
if(ENABLE_SHM)
  target_include_directories(
    cunit_ddsc PRIVATE
    "$<BUILD_INTERFACE:$<TARGET_PROPERTY:iceoryx_binding_c::iceoryx_binding_c,INTERFACE_INCLUDE_DIRECTORIES>>")
endif()

target_link_libraries(cunit_ddsc PRIVATE
  RoundTrip Space TypesArrayKey WriteTypes InstanceHandleTypes RWData CreateWriter DataRepresentationTypes MinXcdrVersion CdrStreamOptimize CdrStreamSkipDefault ddsc)

if(ENABLE_TYPE_DISCOVERY)
  target_link_libraries(cunit_ddsc PRIVATE
  XSpace XSpaceNoTypeInfo TypeBuilderTypes)
endif()

# Setup environment for config-tests
get_test_property(CUnit_ddsc_config_simple_udp ENVIRONMENT CUnit_ddsc_config_simple_udp_env)
set(CUnit_ddsc_config_simple_udp_file "${CMAKE_CURRENT_LIST_DIR}/config_simple_udp.xml")
set(CUnit_ddsc_config_simple_udp_uri "file://${CUnit_ddsc_config_simple_udp_file}")
set(CUnit_ddsc_config_simple_udp_max_participants "0")
set(CUnit_ddsc_config_simple_udp_env "CYCLONEDDS_URI=${CUnit_ddsc_config_simple_udp_uri};MAX_PARTICIPANTS=${CUnit_ddsc_config_simple_udp_max_participants};${CUnit_ddsc_config_simple_udp_env}")
set_tests_properties(
	CUnit_ddsc_config_simple_udp
	PROPERTIES
		REQUIRED_FILES ${CUnit_ddsc_config_simple_udp_file}
		ENVIRONMENT "${CUnit_ddsc_config_simple_udp_env}")

configure_file("deadline_update.h.in" "deadline_update.h")

if(ENABLE_DEADLINE_MISSED)
  foreach(t
      basic
      instances
      update
      writer_types)
    set_tests_properties(CUnit_ddsc_deadline_${t} PROPERTIES RUN_SERIAL TRUE)
  endforeach()
endif()

configure_file("config_env.h.in" "config_env.h" @ONLY)

add_executable(oneliner
  "oneliner.c"
  "test_oneliner.c"
  "test_oneliner.h"
  "test_util.c"
  "test_util.h")
target_include_directories(
  oneliner PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/include/>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsc/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../cdr/include>")

if(ENABLE_SHM)
  target_include_directories(
    oneliner PRIVATE
    "$<BUILD_INTERFACE:$<TARGET_PROPERTY:iceoryx_binding_c::iceoryx_binding_c,INTERFACE_INCLUDE_DIRECTORIES>>")
endif()

target_link_libraries(oneliner PRIVATE RoundTrip Space ddsc)

# Iceoryx itself isn't really supported yet on Windows, so it
# better not be part of the tests.  That also saves us from
# having to figure out now how to start/stop RouDi on Windows.
if(ENABLE_SHM AND NOT DEFINED ENV{COLCON})
  idlc_generate(TARGET Array100 FILES Array100.idl WARNINGS no-implicit-extensibility)
  idlc_generate(TARGET DynamicData FILES DynamicData.idl WARNINGS no-implicit-extensibility)

  add_cunit_executable(cunit_ddsc_iox
    "iceoryx.c"
    "test_util.c"
    "test_util.h")
  target_include_directories(
    cunit_ddsc_iox PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/include/>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsc/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/src>"
    "$<BUILD_INTERFACE:$<TARGET_PROPERTY:iceoryx_binding_c::iceoryx_binding_c,INTERFACE_INCLUDE_DIRECTORIES>>")

  target_link_libraries(cunit_ddsc_iox PRIVATE RoundTrip Space Array100 DynamicData ddsc)

  add_cunit_executable(cunit_ddsc_shm_serialization
    "test_shm_serialization.c"
    "test_util.c"
    "test_util.h")
  target_include_directories(
    cunit_ddsc_shm_serialization PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/include/>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsc/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/include>"
    "$<BUILD_INTERFACE:$<TARGET_PROPERTY:iceoryx_binding_c::iceoryx_binding_c,INTERFACE_INCLUDE_DIRECTORIES>>")

  target_link_libraries(cunit_ddsc_shm_serialization PRIVATE RoundTrip Space DynamicData ddsc)

  # We need to start RouDi, so we need to find it
  find_program(ICEORYX_ROUDI iox-roudi REQUIRED)

  # The run-time library paths aren't set in iox-roudi so we need to set
  # (DY)LD_LIBRARY_PATH.  CTest can't start a daemon, so we have to use
  # a wrapper to start a process, for which bash makes the most sense
  # when targetting just Linux and macOS.  However, that means macOS
  # will strip DYLD_LIBRARY_PATH from the environment and therefore we
  # cannot rely on "set_test_library_paths".
  find_library(ICEORYX_LIB iceoryx_binding_c)
  get_filename_component(ICEORYX_LIB_PATH ${ICEORYX_LIB} DIRECTORY)
  if(APPLE)
    set(ICEORYX_ROUDI_LIBPATH "DYLD_LIBRARY_PATH=${ICEORYX_LIB_PATH}:$ENV{DYLD_LIBRARY_PATH}")
  else()
    set(ICEORYX_ROUDI_LIBPATH "LD_LIBRARY_PATH=${ICEORYX_LIB_PATH}:$ENV{LD_LIBRARY_PATH}")
  endif()
  # CTest waits until the child process terminates *AND* the stdin/stdout/stderr
  # pipes are closed.  Redirecting the output to a file solves that problem.
  #
  # Write the process id of RouDi to ctest_fixture_iox_roudi.pid so we know what
  # process to kill in the cleanup phase.
  #
  # Sleep + kill -0 to check that RouDi really did start, cat'ing the log file in case
  # it failed to start.
  add_test(NAME start_roudi COMMAND
    bash -c
    "\
${ICEORYX_ROUDI_LIBPATH} ${ICEORYX_ROUDI} -c ${CMAKE_CURRENT_SOURCE_DIR}/iox_roudi_config.toml \
  </dev/null >ctest_fixture_iox_roudi.output 2>&1 & echo $! > ctest_fixture_iox_roudi.pid ; \
sleep 1 ; \
cat ctest_fixture_iox_roudi.output ; \
kill -0 `cat ctest_fixture_iox_roudi.pid`")
  set_test_library_paths(start_roudi)

  # SIGTERM should suffice for stopping RouDi
  add_test(NAME stop_roudi COMMAND bash -c "kill `cat ctest_fixture_iox_roudi.pid` ; cat ctest_fixture_iox_roudi.output")

  set_tests_properties(start_roudi PROPERTIES FIXTURES_SETUP iox)
  set_tests_properties(stop_roudi PROPERTIES FIXTURES_CLEANUP iox)
  set_tests_properties(start_roudi stop_roudi PROPERTIES RESOURCE_LOCK iox_lock)

  foreach(t
      iceoryx_one_writer
      iceoryx_one_writer_dynsize
      iceoryx_one_writer_dynsize_strkey
      iceoryx_return_loan
      iceoryx_partition_xtalk
      shm_serialization_get_serialized_size
      shm_serialization_serialize_into
      shm_serialization_transmit_dynamic_type)
    set_tests_properties(CUnit_ddsc_${t} PROPERTIES FIXTURES_REQUIRED iox)
    set_tests_properties(CUnit_ddsc_${t} PROPERTIES RESOURCE_LOCK iox_lock)
  endforeach()
endif()
