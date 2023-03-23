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

set(srcs_ddsi
  ddsi_eth.c
  ddsi_ssl.c
  ddsi_tcp.c
  ddsi_tran.c
  ddsi_udp.c
  ddsi_raweth.c
  ddsi_vnet.c
  ddsi_ipaddr.c
  ddsi_mcgroup.c
  ddsi_nwpart.c
  ddsi_security_util.c
  ddsi_security_omg.c
  ddsi_portmapping.c
  ddsi_handshake.c
  ddsi_serdata.c
  ddsi_serdata_cdr.c
  ddsi_serdata_pserop.c
  ddsi_serdata_plist.c
  ddsi_sertype.c
  ddsi_sertype_cdr.c
  ddsi_sertype_pserop.c
  ddsi_sertype_plist.c
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
  ddsi_config.c
  ddsi_time.c
  ddsi_ownip.c
  ddsi_acknack.c
  ddsi_list_genptr.c
  ddsi_wraddrset.c
  ddsi_typelib.c
  ddsi_entity.c
  ddsi_endpoint_match.c
  ddsi_participant.c
  ddsi_proxy_participant.c
  ddsi_topic.c
  ddsi_endpoint.c
  ddsi_proxy_endpoint.c
  ddsi_gc.c
  ddsi_addrset.c
  ddsi_bitset.c
  ddsi_guid.c
  ddsi_bswap.c
  ddsi_discovery.c
  ddsi_discovery_addrset.c
  ddsi_discovery_spdp.c
  ddsi_discovery_endpoint.c
  ddsi_debmon.c
  ddsi_init.c
  ddsi_lat_estim.c
  ddsi_lease.c
  ddsi_misc.c
  ddsi_pcap.c
  ddsi_qosmatch.c
  ddsi_radmin.c
  ddsi_receive.c
  ddsi_sockwaitset.c
  ddsi_sysdeps.c
  ddsi_thread.c
  ddsi_transmit.c
  ddsi_inverse_uint32_set.c
  ddsi_whc.c
  ddsi_xevent.c
  ddsi_xmsg.c
  ddsi_freelist.c
  ddsi_hbcontrol.c
)

set(hdrs_ddsi
  ddsi_tran.h
  ddsi_locator.h
  ddsi_security_omg.h
  ddsi_portmapping.h
  ddsi_serdata.h
  ddsi_sertype.h
  ddsi_statistics.h
  ddsi_iid.h
  ddsi_tkmap.h
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
  ddsi_ownip.h
  ddsi_config.h
  ddsi_typelib.h
  ddsi_entity.h
  ddsi_endpoint_match.h
  ddsi_participant.h
  ddsi_proxy_participant.h
  ddsi_topic.h
  ddsi_endpoint.h
  ddsi_proxy_endpoint.h
  ddsi_gc.h
  ddsi_pmd.h
  ddsi_protocol.h
  ddsi_addrset.h
  ddsi_feature_check.h
  ddsi_freelist.h
  ddsi_hbcontrol.h
  ddsi_inverse_uint32_set.h
  ddsi_lat_estim.h
  ddsi_lease.h
  ddsi_log.h
  ddsi_qosmatch.h
  ddsi_radmin.h
  ddsi_init.h
  ddsi_sockwaitset.h
  ddsi_thread.h
  ddsi_transmit.h
  ddsi_unused.h
  ddsi_whc.h
  ddsi_xevent.h
  ddsi_xmsg.h
)

set(hdrs_private_ddsi
  ddsi__acknack.h
  ddsi__cfgunits.h
  ddsi__cfgelems.h
  ddsi__config_impl.h
  ddsi__deliver_locally.h
  ddsi__endpoint.h
  ddsi__entity_index.h
  ddsi__endpoint_match.h
  ddsi__entity.h
  ddsi__gc.h
  ddsi__handshake.h
  ddsi__ipaddr.h
  ddsi__list_tmpl.h
  ddsi__list_genptr.h
  ddsi__mcgroup.h
  ddsi__nwpart.h
  ddsi__ownip.h
  ddsi__participant.h
  ddsi__plist_context_kind.h
  ddsi__plist_generic.h
  ddsi__serdata_pserop.h
  ddsi__pmd.h
  ddsi__plist.h
  ddsi__portmapping.h
  ddsi__proxy_endpoint.h
  ddsi__proxy_participant.h
  ddsi__raweth.h
  ddsi__rhc.h
  ddsi__security_omg.h
  ddsi__security_util.h
  ddsi__serdata_cdr.h
  ddsi__serdata_plist.h
  ddsi__ssl.h
  ddsi__sysdeps.h
  ddsi__tcp.h
  ddsi__protocol.h
  ddsi__time.h
  ddsi__topic.h
  ddsi__tran.h
  ddsi__typelib.h
  ddsi__udp.h
  ddsi__vendor.h
  ddsi__vnet.h
  ddsi__wraddrset.h
  ddsi__xqos.h
  ddsi__addrset.h
  ddsi__bitset.h
  ddsi__bswap.h
  ddsi__discovery.h
  ddsi__discovery_addrset.h
  ddsi__discovery_spdp.h
  ddsi__discovery_endpoint.h
  ddsi__debmon.h
  ddsi__hbcontrol.h
  ddsi__inverse_uint32_set.h
  ddsi__lat_estim.h
  ddsi__lease.h
  ddsi__misc.h
  ddsi__pcap.h
  ddsi__radmin.h
  ddsi__receive.h
  ddsi__sockwaitset.h
  ddsi__thread.h
  ddsi__transmit.h
  ddsi__whc.h
  ddsi__xevent.h
  ddsi__xmsg.h
  ddsi__log.h
)

if(ENABLE_LIFESPAN)
  list(APPEND srcs_ddsi
    ddsi_lifespan.c
  )
  list(APPEND hdrs_ddsi
    ddsi_lifespan.h
  )
endif()
if(ENABLE_DEADLINE_MISSED)
  list(APPEND srcs_ddsi
    ddsi_deadline.c
  )
  list(APPEND hdrs_ddsi
    ddsi_deadline.h
  )
endif()
if(ENABLE_TOPIC_DISCOVERY)
  list(APPEND srcs_ddsi
    ddsi_discovery_topic.c
  )
  list(APPEND hdrs_private_ddsi
    ddsi__discovery_topic.h
  )
endif()
if(ENABLE_TYPE_DISCOVERY)
  list(APPEND srcs_ddsi
    ddsi_xt_typeinfo.c
    ddsi_xt_typelookup.c
    ddsi_xt_typemap.c
    ddsi_typelookup.c
    ddsi_typewrap.c
    ddsi_typebuilder.c
    ddsi_dynamic_type.c
  )
  list(APPEND hdrs_ddsi
    ddsi_xt_typeinfo.h
    ddsi_xt_typelookup.h
    ddsi_xt_typemap.h
    ddsi_typewrap.h
    ddsi_typebuilder.h
    ddsi_dynamic_type.h
  )
  list(APPEND hdrs_private_ddsi
    ddsi__xt_impl.h
    ddsi__typelookup.h
    ddsi__dynamic_type.h
  )
endif()
if(ENABLE_SECURITY)
  list(APPEND srcs_ddsi
    ddsi_security_msg.c
    ddsi_security_exchange.c
  )
  list(APPEND hdrs_ddsi
    ddsi_security_msg.h
  )
  list(APPEND hdrs_private_ddsi
    ddsi__security_msg.h
    ddsi__security_exchange.h
  )
endif()
if(ENABLE_SHM)
  list(APPEND srcs_ddsi
    ddsi_shm_transport.c
  )
  list(APPEND hdrs_ddsi
    ddsi_shm_transport.h
  )
endif()

prepend(hdrs_ddsi "${CMAKE_CURRENT_LIST_DIR}/include/dds/ddsi/" ${hdrs_ddsi})
prepend(hdrs_private_ddsi "${CMAKE_CURRENT_LIST_DIR}/src/" ${hdrs_private_ddsi})
prepend(srcs_ddsi "${CMAKE_CURRENT_LIST_DIR}/src/" ${srcs_ddsi})

target_sources(ddsc PRIVATE ${srcs_ddsi} ${generated_defconfig_src} ${hdrs_private_ddsi} ${hdrs_ddsi})
add_dependencies(ddsc _confgen)
target_include_directories(ddsc PRIVATE "${CMAKE_CURRENT_LIST_DIR}/include")

install(
  DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT dev)

# TODO: improve test inclusion.
if((BUILD_TESTING) AND ((NOT DEFINED MSVC_VERSION) OR (MSVC_VERSION GREATER "1800")))
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/tests")
endif()
