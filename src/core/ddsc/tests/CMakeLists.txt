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
idlc_generate(TARGET CdrStreamChecking FILES CdrStreamChecking.idl)
idlc_generate(TARGET CdrStreamWstring FILES CdrStreamWstring.idl)
idlc_generate(TARGET CdrStreamParamHeader FILES CdrStreamParamHeader.idl)
idlc_generate(TARGET CdrStreamSerDes FILES CdrStreamSerDes.idl NO_TYPE_INFO WARNINGS no-enum-consecutive)
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
    "psmx.c"
    "psmxif.c"
    "publisher.c"
    "qos.c"
    "qosmatch.c"
    "qos_set_match.c"
    "querycondition.c"
    "guardcondition.c"
    "matchstress.c"
    "readcollect.c"
    "readcondition.c"
    "reader.c"
    "reader_iterator.c"
    "read_instance.c"
    "redundantnw.c"
    "register.c"
    "spdp.c"
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
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
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
  CdrStreamChecking
  CdrStreamWstring
  CdrStreamParamHeader
  CdrStreamSerDes
  PsmxDataModels
  psmx_dummy
  psmx_dummy_v0
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
configure_file("config_env.h.in" "config_env.h" @ONLY)

########################################################################
# Enfore serial execution for some tests                               #
#                                                                      #
# Serialize some really problematic tests: those relying on unreliable #
# communication and the deadline tests with their tight timing         #
########################################################################
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

#########################
# "oneliner" executable #
#########################
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

##############################################################
# PSMX implementation with Cyclone as transport, for testing #
# PSMX dummy implementation, for interface testing           #
##############################################################
idlc_generate(TARGET psmx_cdds_data FILES psmx_cdds_data.idl)
set(psmx_cdds_sources
  "psmx_cdds_impl.c"
  "psmx_cdds_impl.h")
set(psmx_dummy_sources
  "psmx_dummy_impl.c"
  "psmx_dummy_impl.h"
  "psmx_dummy_public.h")
set(psmx_dummy_v0_sources
  "psmx_dummy_v0_impl.c"
  "psmx_dummy_v0_impl.h"
  "psmx_dummy_v0_public.h")
set(psmx_includes
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../cdr/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsc/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/include>")
if(BUILD_SHARED_LIBS)
  add_library(psmx_cdds SHARED ${psmx_cdds_sources})
  add_library(psmx_dummy SHARED ${psmx_dummy_sources})
  add_library(psmx_dummy_v0 SHARED ${psmx_dummy_v0_sources})
else()
  list(APPEND psmx_includes
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/ddsrt/include>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/core/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsc/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../ddsrt/include>")
  add_library(psmx_cdds OBJECT ${psmx_cdds_sources} psmx_cdds_data.c)
  set_property(GLOBAL APPEND PROPERTY cdds_plugin_list psmx_cdds)
  set_property(GLOBAL PROPERTY psmx_cdds_symbols cdds_create_psmx)
  add_library(psmx_dummy OBJECT ${psmx_dummy_sources})
  set_property(GLOBAL APPEND PROPERTY cdds_plugin_list psmx_dummy)
  set_property(GLOBAL PROPERTY psmx_dummy_symbols dummy_create_psmx)
  add_library(psmx_dummy_v0 OBJECT ${psmx_dummy_v0_sources})
  set_property(GLOBAL APPEND PROPERTY cdds_plugin_list psmx_dummy_v0)
  set_property(GLOBAL PROPERTY psmx_dummy_v0_symbols dummy_v0_create_psmx)
endif()
generate_export_header(psmx_cdds BASE_NAME PSMX_CDDS EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/dds/psmx_cdds/export.h")
generate_export_header(psmx_dummy BASE_NAME PSMX_DUMMY EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/dds/psmx_dummy/export.h")
generate_export_header(psmx_dummy_v0 BASE_NAME PSMX_DUMMY EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/dds/psmx_dummy_v0/export.h")
foreach(plugin psmx_cdds psmx_dummy psmx_dummy_v0)
  target_include_directories(${plugin} PRIVATE ${psmx_includes})
endforeach()
if(BUILD_SHARED_LIBS)
  target_link_libraries(psmx_cdds PRIVATE ddsc psmx_cdds_data)
  target_link_libraries(psmx_dummy PRIVATE ddsc)
  target_link_libraries(psmx_dummy_v0 PRIVATE ddsc)
else()
  install(
    TARGETS psmx_cdds psmx_dummy psmx_dummy_v0
    EXPORT "${PROJECT_NAME}"
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

########################################################################
# If Iceoryx is available, then also run all PSMX tests using Iceoryx. #
# Colcon complicates things too much and it doesn't affect Cyclone's   #
# CI run anyway.                                                       #
########################################################################
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
