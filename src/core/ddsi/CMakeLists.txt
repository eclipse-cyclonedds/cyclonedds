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
  ddsi_entity_match.c
  ddsi_participant.c
  ddsi_proxy_participant.c
  ddsi_topic.c
  ddsi_endpoint.c
  ddsi_proxy_endpoint.c
  ddsi_gc.c
  q_addrset.c
  q_bitset_inlines.c
  q_bswap.c
  q_ddsi_discovery.c
  q_debmon.c
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
  sysdeps.c
)

set(hdrs_ddsi
  ddsi_ssl.h
  ddsi_tcp.h
  ddsi_tran.h
  ddsi_udp.h
  ddsi_vnet.h
  ddsi_locator.h
  ddsi_security_omg.h
  ddsi_portmapping.h
  ddsi_serdata.h
  ddsi_sertype.h
  ddsi_serdata_plist.h
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
  ddsi_time.h
  ddsi_ownip.h
  ddsi_cfgunits.h
  ddsi_cfgelems.h
  ddsi_config.h
  ddsi_config_impl.h
  ddsi_wraddrset.h
  ddsi_typelib.h
  ddsi_entity.h
  ddsi_entity_match.h
  ddsi_participant.h
  ddsi_proxy_participant.h
  ddsi_topic.h
  ddsi_endpoint.h
  ddsi_proxy_endpoint.h
  ddsi_gc.h
  ddsi_pmd.h
  q_addrset.h
  q_bitset.h
  q_bswap.h
  q_ddsi_discovery.h
  q_debmon.h
  q_feature_check.h
  q_freelist.h
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
  sysdeps.h
)

set(hdrs_private_ddsi
  ddsi__acknack.h
  ddsi__config_impl.h
  ddsi__deliver_locally.h
  ddsi__endpoint.h
  ddsi__entity_index.h
  ddsi__entity_match.h
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
if(ENABLE_TYPE_DISCOVERY)
  list(APPEND srcs_ddsi
    ddsi_xt_typeinfo.c
    ddsi_xt_typelookup.c
    ddsi_xt_typemap.c
    ddsi_typelookup.c
    ddsi_typewrap.c
    ddsi_typebuilder.c
  )
  list(APPEND hdrs_ddsi
    ddsi_xt_typeinfo.h
    ddsi_xt_typelookup.h
    ddsi_xt_typemap.h
    ddsi_typelookup.h
    ddsi_typewrap.h
    ddsi_typebuilder.h
  )
  list(APPEND hdrs_private_ddsi
    ddsi__xt_impl.h
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
