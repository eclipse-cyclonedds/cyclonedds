/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef Q_QOSMATCH_H
#define Q_QOSMATCH_H

#if defined (__cplusplus)
extern "C" {
#endif

struct nn_xqos;

int partition_match_based_on_wildcard_in_left_operand (const struct nn_xqos *a, const struct nn_xqos *b, const char **realname);
int partitions_match_p (const struct nn_xqos *a, const struct nn_xqos *b);
int is_wildcard_partition (const char *str);

/* Returns -1 on success, or QoS id of first mismatch (>=0) */

int32_t qos_match_p (const struct nn_xqos *rd, const struct nn_xqos *wr);

#if defined (__cplusplus)
}
#endif

#endif /* Q_QOSMATCH_H */
