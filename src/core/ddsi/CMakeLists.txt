#
# Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
set(defconfig_src "${CMAKE_CURRENT_BINARY_DIR}/defconfig.c")

add_custom_command(
  OUTPUT ${defconfig_src}
  COMMAND ddsconf -f defconfig -o ${defconfig_src}
  DEPENDS ddsconf)

set(srcs_ddsi
  ddsi_eth.c
  ddsi_ssl.c
  ddsi_tcp.c
  ddsi_tran.c
  ddsi_udp.c
  ddsi_raweth.c
  ddsi_ipaddr.c
  ddsi_mcgroup.c
  ddsi_security_util.c
  ddsi_security_omg.c
  ddsi_portmapping.c
  ddsi_handshake.c
  ddsi_serdata.c
  ddsi_serdata_default.c
  ddsi_serdata_pserop.c
  ddsi_serdata_plist.c
  ddsi_sertype.c
  ddsi_sertype_default.c
  ddsi_sertype_pserop.c
  ddsi_sertype_plist.c
  ddsi_sertopic.c
  ddsi_statistics.c
  ddsi_iid.c
  ddsi_tkmap.c
  ddsi_vendor.c
  ddsi_threadmon.c
  ddsi_rhc.c
  ddsi_pmd.c
  ddsi_entity_index.c
  ddsi_deadline.c
  ddsi_deliver_locally.c
  ddsi_plist.c
  ddsi_cdrstream.c
  ddsi_time.c
  ddsi_ownip.c
  ddsi_acknack.c
  q_addrset.c
  q_bitset_inlines.c
  q_bswap.c
  q_config.c
  q_ddsi_discovery.c
  q_debmon.c
  q_entity.c
  q_gc.c
  q_init.c
  q_lat_estim.c
  q_lease.c
  q_misc.c
  q_pcap.c
  q_qosmatch.c
  q_radmin.c
  q_receive.c
  q_sockwaitset.c
  q_thread.c
  q_transmit.c
  q_inverse_uint32_set.c
  q_whc.c
  q_xevent.c
  q_xmsg.c
  q_freelist.c
  sysdeps.c)

set(hdrs_private_ddsi
  ddsi_ssl.h
  ddsi_tcp.h
  ddsi_tran.h
  ddsi_udp.h
  ddsi_raweth.h
  ddsi_ipaddr.h
  ddsi_locator.h
  ddsi_mcgroup.h
  ddsi_plist_generic.h
  ddsi_security_util.h
  ddsi_security_omg.h
  ddsi_portmapping.h
  ddsi_handshake.h
  ddsi_serdata.h
  ddsi_sertype.h
  ddsi_serdata_default.h
  ddsi_serdata_pserop.h
  ddsi_serdata_plist.h
  ddsi_sertopic.h
  ddsi_statistics.h
  ddsi_iid.h
  ddsi_tkmap.h
  ddsi_vendor.h
  ddsi_threadmon.h
  ddsi_builtin_topic_if.h
  ddsi_rhc.h
  ddsi_guid.h
  ddsi_keyhash.h
  ddsi_entity_index.h
  ddsi_deadline.h
  ddsi_deliver_locally.h
  ddsi_domaingv.h
  ddsi_plist.h
  ddsi_xqos.h
  ddsi_cdrstream.h
  ddsi_time.h
  ddsi_ownip.h
  ddsi_cfgunits.h
  ddsi_cfgelems.h
  ddsi_config.h
  ddsi_acknack.h
  q_addrset.h
  q_bitset.h
  q_bswap.h
  q_config.h
  q_ddsi_discovery.h
  q_debmon.h
  q_entity.h
  q_feature_check.h
  q_freelist.h
  q_gc.h
  q_hbcontrol.h
  q_lat_estim.h
  q_lease.h
  q_log.h
  q_misc.h
  q_pcap.h
  q_protocol.h
  q_qosmatch.h
  q_radmin.h
  q_receive.h
  q_rtps.h
  q_sockwaitset.h
  q_thread.h
  q_transmit.h
  q_inverse_uint32_set.h
  q_unused.h
  q_whc.h
  q_xevent.h
  q_xmsg.h
  sysdeps.h)

if(ENABLE_LIFESPAN)
  list(APPEND srcs_ddsi ddsi_lifespan.c)
  list(APPEND hdrs_private_ddsi ddsi_lifespan.h)
endif()
if(ENABLE_DEADLINE_MISSED)
  list(APPEND srcs_ddsi ddsi_deadline.c)
  list(APPEND hdrs_private_ddsi ddsi_deadline.h)
endif()
if(ENABLE_TYPE_DISCOVERY)
  list(APPEND srcs_ddsi ddsi_typeid.c ddsi_typelookup.c)
  list(APPEND hdrs_private_ddsi ddsi_typeid.h ddsi_typelookup.h)
endif()
if(ENABLE_SECURITY)
  list(APPEND srcs_ddsi ddsi_security_msg.c ddsi_security_exchange.c)
  list(APPEND hdrs_private_ddsi ddsi_security_msg.h ddsi_security_exchange.h)
endif()

list(TRANSFORM srcs_ddsi PREPEND "${CMAKE_CURRENT_LIST_DIR}/src/")
list(TRANSFORM hdrs_private_ddsi PREPEND "${CMAKE_CURRENT_LIST_DIR}/include/dds/ddsi/")

target_sources(ddsc PRIVATE ${srcs_ddsi} ${defconfig_src} ${hdrs_private_ddsi})
target_include_directories(ddsc PRIVATE "${CMAKE_CURRENT_LIST_DIR}/include")
install(
  DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT dev)

# TODO: improve test inclusion.
if((BUILD_TESTING) AND ((NOT DEFINED MSVC_VERSION) OR (MSVC_VERSION GREATER "1800")))
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/tests")
endif()
