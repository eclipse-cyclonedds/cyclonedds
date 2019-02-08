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
PREPEND(srcs_ddsi "${CMAKE_CURRENT_LIST_DIR}/src"
    ddsi_eth.c
    ddsi_ssl.c
    ddsi_tcp.c
    ddsi_tran.c
    ddsi_udp.c
    ddsi_raweth.c
    ddsi_ipaddr.c
    ddsi_mcgroup.c
    ddsi_serdata.c
    ddsi_serdata_default.c
    ddsi_sertopic.c
    ddsi_sertopic_default.c
    ddsi_rhc_plugin.c
    ddsi_iid.c
    ddsi_tkmap.c
    ddsi_vendor.c
    q_addrset.c
    q_bitset_inlines.c
    q_bswap.c
    q_bswap_inlines.c
    q_config.c
    q_ddsi_discovery.c
    q_debmon.c
    q_entity.c
    q_ephash.c
    q_gc.c
    q_init.c
    q_lat_estim.c
    q_lease.c
    q_md5.c
    q_misc.c
    q_nwif.c
    q_pcap.c
    q_plist.c
    q_qosmatch.c
    q_radmin.c
    q_receive.c
    q_security.c
    q_servicelease.c
    q_sockwaitset.c
    q_thread.c
    q_thread_inlines.c
    q_time.c
    q_transmit.c
    q_inverse_uint32_set.c
    q_whc.c
    q_xevent.c
    q_xmsg.c
    q_freelist.c
    sysdeps.c
)

# The includes should reside close to the code. As long as that's not the case,
# pull them in from this CMakeLists.txt.
PREPEND(hdrs_private_ddsi "${CMAKE_CURRENT_LIST_DIR}/include/ddsi"
    ddsi_ssl.h
    ddsi_tcp.h
    ddsi_tran.h
    ddsi_udp.h
    ddsi_raweth.h
    ddsi_ipaddr.h
    ddsi_mcgroup.h
    ddsi_serdata.h
    ddsi_sertopic.h
    ddsi_serdata_default.h
    ddsi_rhc_plugin.h
    ddsi_iid.h
    ddsi_tkmap.h
    ddsi_vendor.h
    probes-constants.h
    q_addrset.h
    q_bitset.h
    q_bswap.h
    q_config.h
    q_ddsi_discovery.h
    q_debmon.h
    q_entity.h
    q_ephash.h
    q_error.h
    q_feature_check.h
    q_freelist.h
    q_gc.h
    q_globals.h
    q_hbcontrol.h
    q_lat_estim.h
    q_lease.h
    q_log.h
    q_md5.h
    q_misc.h
    q_nwif.h
    q_pcap.h
    q_plist.h
    q_protocol.h
    q_qosmatch.h
    q_radmin.h
    q_receive.h
    q_rtps.h
    q_security.h
    q_servicelease.h
    q_sockwaitset.h
    q_static_assert.h
    q_thread.h
    q_time.h
    q_transmit.h
    q_inverse_uint32_set.h
    q_unused.h
    q_whc.h
    q_xevent.h
    q_xmsg.h
    q_xqos.h
    sysdeps.h
)

target_sources(ddsc
    PRIVATE
        ${srcs_ddsi}
        ${hdrs_private_ddsi}
)

target_include_directories(ddsc
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/include")
