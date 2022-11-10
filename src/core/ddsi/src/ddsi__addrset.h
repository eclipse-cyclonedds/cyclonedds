/*
 * Copyright(c) 2006 to 2022 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSI__ADDRSET_H
#define DDSI__ADDRSET_H

#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/ddsi_protocol.h"
#include "dds/ddsi/q_feature_check.h"
#include "dds/ddsi/ddsi_addrset.h"
#include "dds/ddsi/ddsi_locator.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_domaingv;

typedef ssize_t (*ddsi_addrset_forone_fun_t) (const ddsi_xlocator_t *loc, void *arg);

struct ddsi_addrset *ddsi_new_addrset (void);
struct ddsi_addrset *ddsi_ref_addrset (struct ddsi_addrset *as);
void ddsi_unref_addrset (struct ddsi_addrset *as);
void ddsi_add_locator_to_addrset (const struct ddsi_domaingv *gv, struct ddsi_addrset *as, const ddsi_locator_t *loc);
void ddsi_add_xlocator_to_addrset (const struct ddsi_domaingv *gv, struct ddsi_addrset *as, const ddsi_xlocator_t *loc);
void ddsi_remove_from_addrset (const struct ddsi_domaingv *gv, struct ddsi_addrset *as, const ddsi_xlocator_t *loc);
int ddsi_addrset_purge (struct ddsi_addrset *as);
int ddsi_compare_locators (const ddsi_locator_t *a, const ddsi_locator_t *b);
int ddsi_compare_xlocators (const ddsi_xlocator_t *a, const ddsi_xlocator_t *b);

/* These lock ASADD, then lock/unlock AS any number of times, then
   unlock ASADD */
void ddsi_copy_addrset_into_addrset_uc (const struct ddsi_domaingv *gv, struct ddsi_addrset *as, const struct ddsi_addrset *asadd);
void ddsi_copy_addrset_into_addrset_mc (const struct ddsi_domaingv *gv, struct ddsi_addrset *as, const struct ddsi_addrset *asadd);
void ddsi_copy_addrset_into_addrset (const struct ddsi_domaingv *gv, struct ddsi_addrset *as, const struct ddsi_addrset *asadd);

size_t ddsi_addrset_count (const struct ddsi_addrset *as);
size_t ddsi_addrset_count_uc (const struct ddsi_addrset *as);
size_t ddsi_addrset_count_mc (const struct ddsi_addrset *as);
int ddsi_addrset_empty_uc (const struct ddsi_addrset *as);
int ddsi_addrset_empty_mc (const struct ddsi_addrset *as);
int ddsi_addrset_any_uc (const struct ddsi_addrset *as, ddsi_xlocator_t *dst);
int ddsi_addrset_any_mc (const struct ddsi_addrset *as, ddsi_xlocator_t *dst);
void ddsi_addrset_any_uc_else_mc_nofail (const struct ddsi_addrset *as, ddsi_xlocator_t *dst);

/* Keeps AS locked */
int ddsi_addrset_forone (struct ddsi_addrset *as, ddsi_addrset_forone_fun_t f, void *arg);
size_t ddsi_addrset_forall_count (struct ddsi_addrset *as, ddsi_addrset_forall_fun_t f, void *arg);
size_t ddsi_addrset_forall_uc_else_mc_count (struct ddsi_addrset *as, ddsi_addrset_forall_fun_t f, void *arg);
size_t ddsi_addrset_forall_mc_count (struct ddsi_addrset *as, ddsi_addrset_forall_fun_t f, void *arg);
void ddsi_log_addrset (struct ddsi_domaingv *gv, uint32_t tf, const char *prefix, const struct ddsi_addrset *as);

/* Tries to lock A then B for a decent check, returning false if
   trylock B fails */
int ddsi_addrset_eq_onesidederr (const struct ddsi_addrset *a, const struct ddsi_addrset *b);

int ddsi_is_unspec_locator (const ddsi_locator_t *loc);
int ddsi_is_unspec_xlocator (const ddsi_xlocator_t *loc);
void ddsi_set_unspec_locator (ddsi_locator_t *loc);
void ddsi_set_unspec_xlocator (ddsi_xlocator_t *loc);

int ddsi_add_addresses_to_addrset (const struct ddsi_domaingv *gv, struct ddsi_addrset *as, const char *addrs, int port_mode, const char *msgtag, int req_mc);

#ifdef DDS_HAS_SSM
int ddsi_addrset_contains_ssm (const struct ddsi_domaingv *gv, const struct ddsi_addrset *as);
int ddsi_addrset_any_ssm (const struct ddsi_domaingv *gv, const struct ddsi_addrset *as, ddsi_xlocator_t *dst);
int ddsi_addrset_any_non_ssm_mc (const struct ddsi_domaingv *gv, const struct ddsi_addrset *as, ddsi_xlocator_t *dst);
void ddsi_copy_addrset_into_addrset_no_ssm_mc (const struct ddsi_domaingv *gv, struct ddsi_addrset *as, const struct ddsi_addrset *asadd);
void ddsi_copy_addrset_into_addrset_no_ssm (const struct ddsi_domaingv *gv, struct ddsi_addrset *as, const struct ddsi_addrset *asadd);
#endif

#if defined (__cplusplus)
}
#endif
#endif /* DDSI__ADDRSET_H */
