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
include(Generate)

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
idlc_generate(TARGET CdrStreamKeySize FILES CdrStreamKeySize.idl)
idlc_generate(TARGET CdrStreamKeyExt FILES CdrStreamKeyExt.idl)
idlc_generate(TARGET SerdataData FILES SerdataData.idl)
idlc_generate(TARGET PsmxDataModels FILES PsmxDataModels.idl WARNINGS no-implicit-extensibility)
idlc_generate(TARGET CdrStreamDataTypeInfo FILES CdrStreamDataTypeInfo.idl WARNINGS no-implicit-extensibility)
idlc_generate(TARGET Array100 FILES Array100.idl WARNINGS no-implicit-extensibility)
idlc_generate(TARGET DynamicData FILES DynamicData.idl WARNINGS no-implicit-extensibility)
if(ENABLE_TYPELIB)
  idlc_generate(TARGET XSpace FILES XSpace.idl XSpaceEnum.idl XSpaceMustUnderstand.idl XSpaceTypeConsistencyEnforcement.idl WARNINGS no-implicit-extensibility no-inherit-appendable)
  idlc_generate(TARGET XSpaceNoTypeInfo FILES XSpaceNoTypeInfo.idl NO_TYPE_INFO WARNINGS no-implicit-extensibility)
  idlc_generate(TARGET TypeBuilderTypes FILES TypeBuilderTypes.idl WARNINGS no-implicit-extensibility)
  idlc_generate(TARGET DynamicTypeTypes FILES DynamicTypeTypes.idl)
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
    "qos_set_match.c"
    "querycondition.c"
    "guardcondition.c"
    "readcollect.c"
    "readcondition.c"
    "reader.c"
    "reader_iterator.c"
    "read_instance.c"
    "redundantnw.c"
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
    "serdata_keys.c"
  )

# PSMX tests are tricky in a static build: we have a PSMX plugin that is based on Cyclone
# so we can test the interface even when Iceoryx is not available, but supporting that
# plugin is too complicated in a static build: it introduces a circular dependency and
# that's too hard for me in CMake.
#
# On most platforms (Linux, Windows, macOS) we could still load it dynamically but that
# fails to work because the shared library then includes its own copy of Cyclone DDS and
# then the access to the application readers that the plugin needs is no longer possible
# because they live in the other copy of the library.
#
# Fortunately, running them with Iceoryx doesn't suffer from this.  If we simply skip
# these tests in a static build without Iceoryx and then we ensure the static build on CI
# uses Iceoryx, we should be good.
if(BUILD_SHARED_LIBS OR (ENABLE_ICEORYX AND NOT DEFINED ENV{COLCON}))
  list(APPEND ddsc_test_sources "psmx.c")
endif()

if(ENABLE_LIFESPAN)
  list(APPEND ddsc_test_sources "lifespan.c")
endif()

if(ENABLE_DEADLINE_MISSED)
  list(APPEND ddsc_test_sources "deadline.c")
endif()

if(ENABLE_TYPELIB)
  list(APPEND ddsc_test_sources
    "xtypes_common.c"
    "xtypes_common.h"
    "xtypes_typeinfo.c"
    "data_representation.c"
    "typebuilder.c"
    "dynamic_type.c"
  )
endif()

if(ENABLE_TYPE_DISCOVERY)
  list(APPEND ddsc_test_sources
    "xtypes_typelookup.c"
    "xtypes_assignability.c")
endif()

if(ENABLE_TOPIC_DISCOVERY)
  list(APPEND ddsc_test_sources
    "topic_discovery.c"
    "topic_find_global.c")
endif()

if(ENABLE_QOS_PROVIDER)
  list(APPEND ddsc_test_sources
    "qos_provider.c")
endif()

add_cunit_executable(cunit_ddsc ${ddsc_test_sources})
target_include_directories(
  cunit_ddsc PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/include/>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsc/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../cdr/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../cdr/test>")

target_link_libraries(cunit_ddsc PRIVATE
  RoundTrip
  Space
  TypesArrayKey
  WriteTypes
  InstanceHandleTypes
  RWData
  CreateWriter
  DataRepresentationTypes
  MinXcdrVersion
  CdrStreamOptimize
  CdrStreamSkipDefault
  CdrStreamDataTypeInfo
  PsmxDataModels
  DynamicData
  Array100
  CdrStreamKeySize
  CdrStreamKeyExt
  SerdataData
  ddsc
)


if(ENABLE_TYPELIB)
  target_link_libraries(cunit_ddsc PRIVATE
  XSpace XSpaceNoTypeInfo TypeBuilderTypes DynamicTypeTypes)
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
configure_file("build_options.h.in" "build_options.h")

# Serialize some really problematic tests: those relying on unreliable communication
# and the deadline tests with their tight timing
foreach(t async_one_unrel_sample relwr_unrelrd_network)
  set_tests_properties(CUnit_ddsc_write_${t} PROPERTIES RUN_SERIAL TRUE)
endforeach()
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

target_link_libraries(oneliner PRIVATE RoundTrip Space ddsc)


# PSMX implementation with Cyclone as transport, for testing
if (BUILD_SHARED_LIBS)
  idlc_generate(TARGET psmx_cdds_data FILES psmx_cdds_data.idl)
  set(psmx_cdds_sources
    "psmx_cdds_impl.c"
    "psmx_cdds_impl.h")
  add_library(psmx_cdds SHARED ${psmx_cdds_sources})
  target_include_directories(
    psmx_cdds PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/include>")
  target_link_libraries(psmx_cdds PRIVATE ddsc psmx_cdds_data)

  generate_export_header(psmx_cdds BASE_NAME PSMX_CDDS EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/dds/psmx_cdds/export.h")

  install(TARGETS psmx_cdds
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

# If Iceoryx is available, then also run all PSMX tests using Iceoryx.  Colcon complicates
# things too much and it doesn't affect Cyclone's CI run anyway.
if(ENABLE_ICEORYX AND NOT DEFINED ENV{COLCON})

  # We need to start RouDi, so we need to find it
  find_program(ICEORYX_ROUDI iox-roudi REQUIRED)

  # The run-time library paths aren't set in iox-roudi so we need to set
  # (DY)LD_LIBRARY_PATH.  CTest can't start a daemon, so we have to use
  # a wrapper to start a process, for which bash makes the most sense
  # when targetting just Linux and macOS.  However, that means macOS
  # will strip DYLD_LIBRARY_PATH from the environment and therefore we
  # cannot rely on "set_test_library_paths".
  find_library(ICEORYX_LIB iceoryx_posh)
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

  # Construct Iceoryx-variants of all PSMX tests if building shared libraries, map them to
  # Iceoryx in a static build (because I don't know how to delete tests!)
  get_property(test_names DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY TESTS)
  list(FILTER test_names INCLUDE REGEX "^CUnit_ddsc_psmx_[A-Za-z_0-9]+$")
  if(BUILD_SHARED_LIBS)
    foreach(fullname ${test_names})
      string(REGEX REPLACE "^CUnit_ddsc_psmx_(.*)" "\\1" shortname "${fullname}")
      add_test(NAME ${fullname}_iox COMMAND cunit_ddsc -s ddsc_psmx -t ${shortname})
      set_tests_properties(${fullname}_iox PROPERTIES FIXTURES_REQUIRED iox)
      set_tests_properties(${fullname}_iox PROPERTIES RESOURCE_LOCK iox_lock)
      set_tests_properties(${fullname}_iox PROPERTIES ENVIRONMENT "CDDS_PSMX_NAME=iox;LD_LIBRARY_PATH=$<TARGET_FILE_DIR:psmx_iox>:$ENV{LD_LIBRARY_PATH}")
    endforeach()
  else()
    foreach(fullname ${test_names})
      set_tests_properties(${fullname} PROPERTIES FIXTURES_REQUIRED iox)
      set_tests_properties(${fullname} PROPERTIES RESOURCE_LOCK iox_lock)
      set_tests_properties(${fullname} PROPERTIES ENVIRONMENT "CDDS_PSMX_NAME=iox")
    endforeach()
  endif()
endif()
