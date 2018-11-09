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
#ifndef NN_SERVICELEASE_H
#define NN_SERVICELEASE_H

#if defined (__cplusplus)
extern "C" {
#endif

struct nn_servicelease;

struct nn_servicelease *nn_servicelease_new (void (*renew_cb) (void *arg), void *renew_arg);
int nn_servicelease_start_renewing (struct nn_servicelease *sl);
void nn_servicelease_stop_renewing (struct nn_servicelease *sl);
void nn_servicelease_free (struct nn_servicelease *sl);
void nn_servicelease_statechange_barrier (struct nn_servicelease *sl);

#if defined (__cplusplus)
}
#endif

#endif /* NN_SERVICELEASE_H */
