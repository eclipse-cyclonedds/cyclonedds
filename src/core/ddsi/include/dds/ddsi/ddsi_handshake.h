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
#ifndef DDSI_HANDSHAKE_H
#define DDSI_HANDSHAKE_H

#include "q_entity.h"
#include "ddsi_security_msg.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct participant;
struct proxy_participant;
struct ddsi_handshake;
struct dssi_hsadmin;

#ifdef DDSI_INCLUDE_SECURITY

enum ddsi_handshake_state {
    STATE_HANDSHAKE_IN_PROGRESS,
    STATE_HANDSHAKE_TIMED_OUT,
    STATE_HANDSHAKE_FAILED,
    STATE_HANDSHAKE_PROCESSED,
    STATE_HANDSHAKE_SEND_TOKENS,
    STATE_HANDSHAKE_OK
};

/* The handshake will not use the related handshake object after this callback
 * was executed. This means that it can be deleted in this callback. */
typedef void (*ddsi_handshake_end_cb_t)(
        struct ddsi_handshake *handshake,
        const ddsi_guid_t *lpguid, /* Local participant */
        const ddsi_guid_t *ppguid, /* Proxy participant */
        nn_wctime_t timestamp,
        enum ddsi_handshake_state result);

void ddsi_handshake_start(struct ddsi_handshake *hs);
void ddsi_handshake_release(struct ddsi_handshake *handshake);
struct ddsi_handshake* ddsi_handshake_create(const struct participant *pp, const struct proxy_participant *proxypp, nn_wctime_t timestamp, ddsi_handshake_end_cb_t callback);
void ddsi_handshake_handle_message(struct ddsi_handshake *handshake, const struct participant *pp, const struct proxy_participant *proxypp, const struct nn_participant_generic_message *msg);
void ddsi_handshake_crypto_tokens_received(const struct ddsi_handshake *handshake);
int64_t ddsi_handshake_get_shared_secret(const struct ddsi_handshake *handshake);
int64_t ddsi_handshake_get_handle(const struct ddsi_handshake *handshake);
struct q_globals * ddsi_handshake_get_globals(const struct ddsi_handshake *handshake);

/* The handshake admin (hsadmin) is provided to associate a handshake with the guid
 * of a local participant. Note that this hsadmin will typically be used by each
 * proxy participant to store the handshake information associated with each
 * local participant.
 */

struct ddsi_hsadmin * ddsi_hsadmin_create(void);
void ddsi_hsadmin_delete(struct ddsi_hsadmin *admin);
void ddsi_hsadmin_clear(struct ddsi_hsadmin *admin);
struct ddsi_handshake * ddsi_hsadmin_register_locked(struct ddsi_hsadmin *admin, const struct participant *pp, const struct proxy_participant *proxypp, nn_wctime_t timestamp, ddsi_handshake_end_cb_t callback);
struct ddsi_handshake * ddsi_hsadmin_find(const struct ddsi_hsadmin *admin, const ddsi_guid_t *lguid);
void ddsi_hsadmin_lock(struct ddsi_hsadmin *admin);
void ddsi_hsadmin_unlock(struct ddsi_hsadmin *admin);
void ddsi_hsadmin_remove_by_guid(struct ddsi_hsadmin *admin, const ddsi_guid_t *lguid);
void ddsi_hsadmin_remove_from_fsm(struct ddsi_hsadmin *admin, struct ddsi_handshake *hs);

#else /* DDSI_INCLUDE_SECURITY */

inline void ddsi_handshake_start(UNUSED_ARG(struct ddsi_handshake *hs))
{
}

inline struct ddsi_handshake * ddsi_handshake_create(UNUSED_ARG(const struct participant *pp), UNUSED_ARG(const struct proxy_participant *proxypp), UNUSED_ARG(nn_wctime_t timestamp), UNUSED_ARG(ddsi_handshake_end_cb_t callback))
{
  return NULL;
}

inline int64_t ddsi_handshake_get_shared_secret(UNUSED_ARG(const struct ddsi_handshake *handshake))
{
  return 0;
}

inline int64_t ddsi_handshake_get_handle(UNUSED_ARG(const struct ddsi_handshake *handshake))
{
  return 0;
}

inline struct ddsi_hsadmin * ddsi_hsadmin_create(void)
{
  return NULL;
}

inline void ddsi_handshake_release(UNUSED_ARG(struct ddsi_handshake *handshake))
{
}

inline void ddsi_hsadmin_clear(UNUSED_ARG(struct ddsi_hsadmin *admin))
{
}

inline void ddsi_hsadmin_delete(UNUSED_ARG(struct ddsi_hsadmin *admin))
{
}

inline void ddsi_hsadmin_lock(UNUSED_ARG(struct ddsi_hsadmin *admin))
{
}

inline void ddsi_hsadmin_unlock(UNUSED_ARG(struct ddsi_hsadmin *admin))
{
}

inline struct ddsi_handshake * ddsi_hsadmin_register_locked(UNUSED_ARG(struct ddsi_hsadmin *admin), UNUSED_ARG(const struct participant *pp), UNUSED_ARG(const struct proxy_participant *proxypp), UNUSED_ARG(nn_wctime_t timestamp), UNUSED_ARG(ddsi_handshake_end_cb_t callback))
{
   return NULL;
}

inline struct ddsi_handshake * ddsi_hsadmin_find(UNUSED_ARG(const struct ddsi_hsadmin *admin), UNUSED_ARG(const ddsi_guid_t *lguid))
{
  return NULL;
}

inline void ddsi_hsadmin_remove_by_guid(UNUSED_ARG(struct ddsi_hsadmin *admin), UNUSED_ARG(const nn_guid_t *lguid))
{
}

inline void ddsi_hsadmin_remove_from_fsm(UNUSED_ARG(struct ddsi_hsadmin *admin), UNUSED_ARG(struct ddsi_handshake *hs))
{
}

#endif /* DDSI_INCLUDE_SECURITY */

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_HANDSHAKE_H */
