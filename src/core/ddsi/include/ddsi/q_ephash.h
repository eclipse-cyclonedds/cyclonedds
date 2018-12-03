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
#ifndef Q_EPHASH_H
#define Q_EPHASH_H

#include "os/os_defs.h"
#include "util/ut_hopscotch.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ephash;
struct participant;
struct reader;
struct writer;
struct proxy_participant;
struct proxy_reader;
struct proxy_writer;
struct nn_guid;

  enum entity_kind {
    EK_PARTICIPANT,
    EK_PROXY_PARTICIPANT,
    EK_WRITER,
    EK_PROXY_WRITER,
    EK_READER,
    EK_PROXY_READER
  };
#define EK_NKINDS ((int) EK_PROXY_READER + 1)
  
struct ephash_enum
{
  struct ut_chhIter it;
  enum entity_kind kind;
  struct entity_common *cur;
};

/* Readers & writers are both in a GUID- and in a GID-keyed table. If
   they are in the GID-based one, they are also in the GUID-based one,
   but not the way around, for two reasons:

   - firstly, there are readers & writers that do not have a GID
     (built-in endpoints, fictitious transient data readers),

   - secondly, they are inserted first in the GUID-keyed one, and then
     in the GID-keyed one.

   The GID is used solely for the interface with the OpenSplice
   kernel, all internal state and protocol handling is done using the
   GUID. So all this means is that, e.g., a writer being deleted
   becomes invisible to the network reader slightly before it
   disappears in the protocol handling, or that a writer might exist
   at the protocol level slightly before the network reader can use it
   to transmit data. */

struct ephash *ephash_new (void);
void ephash_free (struct ephash *ephash);

void ephash_insert_participant_guid (struct participant *pp);
void ephash_insert_proxy_participant_guid (struct proxy_participant *proxypp);
void ephash_insert_writer_guid (struct writer *wr);
void ephash_insert_reader_guid (struct reader *rd);
void ephash_insert_proxy_writer_guid (struct proxy_writer *pwr);
void ephash_insert_proxy_reader_guid (struct proxy_reader *prd);

void ephash_remove_participant_guid (struct participant *pp);
void ephash_remove_proxy_participant_guid (struct proxy_participant *proxypp);
void ephash_remove_writer_guid (struct writer *wr);
void ephash_remove_reader_guid (struct reader *rd);
void ephash_remove_proxy_writer_guid (struct proxy_writer *pwr);
void ephash_remove_proxy_reader_guid (struct proxy_reader *prd);

void *ephash_lookup_guid_untyped (const struct nn_guid *guid);
void *ephash_lookup_guid (const struct nn_guid *guid, enum entity_kind kind);

struct participant *ephash_lookup_participant_guid (const struct nn_guid *guid);
struct proxy_participant *ephash_lookup_proxy_participant_guid (const struct nn_guid *guid);
struct writer *ephash_lookup_writer_guid (const struct nn_guid *guid);
struct reader *ephash_lookup_reader_guid (const struct nn_guid *guid);
struct proxy_writer *ephash_lookup_proxy_writer_guid (const struct nn_guid *guid);
struct proxy_reader *ephash_lookup_proxy_reader_guid (const struct nn_guid *guid);


/* Enumeration of entries in the hash table:

   - "next" visits at least all entries that were in the hash table at
     the time of calling init and that have not subsequently been
     removed;

   - "next" may visit an entry more than once, but will do so only
     because of rare events (i.e., resize or so);

   - the order in which entries are visited is arbitrary;

   - the caller must call init() before it may call next(); it must
     call fini() before it may call init() again. */
struct ephash_enum_participant { struct ephash_enum st; };
struct ephash_enum_writer { struct ephash_enum st; };
struct ephash_enum_reader { struct ephash_enum st; };
struct ephash_enum_proxy_participant { struct ephash_enum st; };
struct ephash_enum_proxy_writer { struct ephash_enum st; };
struct ephash_enum_proxy_reader { struct ephash_enum st; };

void ephash_enum_init (struct ephash_enum *st, enum entity_kind kind);
void *ephash_enum_next (struct ephash_enum *st);
void ephash_enum_fini (struct ephash_enum *st);

void ephash_enum_writer_init (struct ephash_enum_writer *st);
void ephash_enum_reader_init (struct ephash_enum_reader *st);
void ephash_enum_proxy_writer_init (struct ephash_enum_proxy_writer *st);
void ephash_enum_proxy_reader_init (struct ephash_enum_proxy_reader *st);
void ephash_enum_participant_init (struct ephash_enum_participant *st);
void ephash_enum_proxy_participant_init (struct ephash_enum_proxy_participant *st);

struct writer *ephash_enum_writer_next (struct ephash_enum_writer *st);
struct reader *ephash_enum_reader_next (struct ephash_enum_reader *st);
struct proxy_writer *ephash_enum_proxy_writer_next (struct ephash_enum_proxy_writer *st);
struct proxy_reader *ephash_enum_proxy_reader_next (struct ephash_enum_proxy_reader *st);
struct participant *ephash_enum_participant_next (struct ephash_enum_participant *st);
struct proxy_participant *ephash_enum_proxy_participant_next (struct ephash_enum_proxy_participant *st);

void ephash_enum_writer_fini (struct ephash_enum_writer *st);
void ephash_enum_reader_fini (struct ephash_enum_reader *st);
void ephash_enum_proxy_writer_fini (struct ephash_enum_proxy_writer *st);
void ephash_enum_proxy_reader_fini (struct ephash_enum_proxy_reader *st);
void ephash_enum_participant_fini (struct ephash_enum_participant *st);
void ephash_enum_proxy_participant_fini (struct ephash_enum_proxy_participant *st);

#if defined (__cplusplus)
}
#endif

#endif /* Q_EPHASH_H */
