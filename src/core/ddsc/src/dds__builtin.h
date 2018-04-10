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
#ifndef _DDS_BUILTIN_H_
#define _DDS_BUILTIN_H_

#include "ddsi/q_time.h"
#include "dds_builtinTopics.h"


#if defined (__cplusplus)
extern "C"
{
#endif



/* Get actual topic in related participant related to topic 'id'. */
_Must_inspect_result_ dds_entity_t
dds__get_builtin_topic(
    _In_ dds_entity_t e,
    _In_ dds_entity_t topic);

/* Global publisher singleton (publishes only locally). */
_Must_inspect_result_ dds_entity_t
dds__get_builtin_publisher(
    void);

/* Subscriber singleton within related participant. */
_Must_inspect_result_ dds_entity_t
dds__get_builtin_subscriber(
    _In_ dds_entity_t e);



/* Initialization and finalize functions. */
void
dds__builtin_init(
        void);

void
dds__builtin_fini(
        void);



/* Callback functions that contain received builtin data. */
void
dds__builtin_participant_cb(
        DDS_ParticipantBuiltinTopicData *data,
        nn_wctime_t timestamp);

void
dds__builtin_cmparticipant_cb(
        DDS_CMParticipantBuiltinTopicData *data,
        nn_wctime_t timestamp);


#if defined (__cplusplus)
}
#endif

#endif /* _DDS_BUILTIN_H_ */

