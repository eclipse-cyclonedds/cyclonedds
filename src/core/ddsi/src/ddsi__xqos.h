// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__XQOS_H
#define DDSI__XQOS_H

#include "dds/features.h"

#include "dds/ddsi/ddsi_protocol.h"
#include "dds/ddsi/ddsi_log.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsc/dds_public_qosdefs.h"
#include "ddsi__plist_context_kind.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_xmsg;

/**
 * @brief Replace any memory "xqos" aliases by copies it owns
 * @component qos_handling
 *
 * A dds_qos_t may can reference other memory without owning it.  This functions allows
 * one to replace any such aliased memory by copies, allowing one to free the original
 * copy.
 *
 * @param[in,out] xqos   qos object for which to replace all aliased memory by owned
 *                       copies
 */
void ddsi_xqos_unalias (dds_qos_t *xqos);

/**
 * @brief Free memory owned by "xqos" for a subset of the entries
 * @component qos_handling
 *
 * A dds_qos_t may own other allocated blocks of memory, depending on which fields are
 * set, their types and whether they are marked as "aliased".  This function releases any
 * such memory owned by "xqos" for entries included in "mask".  The "present" and
 * "aliased" bits are cleared accordingly.
 *
 * @param[in,out] xqos   dds_qos_t for which to free memory
 * @param[in]     mask   entries to free (if DDSI_QP_X is set, free X if present)
 */
void ddsi_xqos_fini_mask (dds_qos_t *xqos, uint64_t mask);

/**
 * @brief Add selected entries in "xqos" to a message in native endianness.
 * @component qos_handling
 *
 * This functions appends to "xqos" a serialized copy of the the entries selected by
 * "wanted" and present in "xqos".  Each copy is preceded by a 4-byte header with a
 * parameter id and length (conform the PL_CDR representation).  It does *not* add a
 * sentinel to allow adding additional data to the parameter list.  A sentinel can be
 * added using `nn_xmsg_addpar_sentinel`.
 *
 * @param[in,out] m        message to append the parameters to
 * @param[in]     xqos     source
 * @param[in]     wanted   subset to be added (if DDSI_QP_X is set, add X if present)
 * @param[in]     context_kind  context for interpretation of QoS settings
 */
void ddsi_xqos_addtomsg (struct ddsi_xmsg *m, const dds_qos_t *xqos, uint64_t wanted, enum ddsi_plist_context_kind context_kind);

/**
 * @brief Formats xqos using `ddsi_xqos_print` and writes it to the trace.
 * @component qos_handling
 *
 * @param[in] cat        log category to use
 * @param[in] logcfg     logging configuration
 * @param[in] xqos       qos object to be logged
 */
void ddsi_xqos_log (uint32_t cat, const struct ddsrt_log_cfg *logcfg, const dds_qos_t *xqos);

/**
 * @brief Formats xqos into a buffer
 * @component qos_handling
 *
 * The representation is somewhat cryptic as all enumerated types are dumped as numbers
 * and timestamps are durations as nanoseconds with "infinite" represented as
 * 9223372036854775807 (INT64_MAX).
 *
 * @param[out] buf       buffer to store the formatted representation in
 * @param[in]  bufsize   size of buffer, if > 0, there will be a terminating 0 in buf on
 *                       return
 * @param[in]  xqos      parameter list to be formatted as a string
 *
 * @returns number of bytes written to buf, excluding a terminating 0.
 */
size_t ddsi_xqos_print (char * __restrict buf, size_t bufsize, const dds_qos_t *xqos);

/**
 * @brief Check if "xqos" includes properties with a name starting with "nameprefix"
 * @component qos_handling
 *
 * That is, if xqos.present has DDSI_QP_PROPERTY_LIST set, and at least one of them has a name
 * starting with "nameprefix".
 *
 * @param[in]  xqos        qos object to check
 * @param[in]  nameprefix  prefix to check for
 *
 * @returns true iff xqos contains a matching property
 */
bool ddsi_xqos_has_prop_prefix (const dds_qos_t *xqos, const char *nameprefix);

/**
 * @brief Lookup property "name" in "xqos" and return a pointer to its value
 * @component qos_handling
 *
 * The value pointer is left unchanged if the property doesn't exist.  The returned
 * address points into the memory owned by the QoS object and must not be freed.
 *
 * @param[in]  xqos        qos object to check
 * @param[in]  name        name to look for
 * @param[out] value       pointer to set to the value of the property if it exists
 *
 * @returns true iff xqos contains the property
 */
bool ddsi_xqos_find_prop (const dds_qos_t *xqos, const char *name, const char **value);

#ifdef DDS_HAS_SECURITY

struct ddsi_config_omg_security;

/** @component qos_handling */
void ddsi_xqos_mergein_security_config (dds_qos_t *xqos, const struct ddsi_config_omg_security *cfg);

#endif /* DDS_HAS_SECURITY */

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__XQOS_H */
