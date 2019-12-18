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
#include <stddef.h>
#include <string.h>
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/q_plist.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_security_msg.h"
#include "dds/ddsi/ddsi_plist_generic.h"

const enum pserop pserop_participant_generic_message[] =
{
  /* nn_participant_generic_message */
  XG, Xl,              /* nn_message_identity_t message_identity         */
  XG, Xl,              /* nn_message_identity_t related_message_identity */
  XG,                  /* ddsi_guid_t destination_participant_guid       */
  XG,                  /* ddsi_guid_t destination_endpoint_guid          */
  XG,                  /* ddsi_guid_t source_endpoint_guid               */
  XS,                  /* char* message_class_id                         */
  XQ,                  /* nn_dataholderseq_t message_data                */
    /* nn_dataholder_t */
    XS,                  /* char* class_id                               */
    XQ,                  /* dds_propertyseq_t properties                 */
      XbPROP, XS, XS,      /* dds_property_t                             */
    XSTOP,
    XQ,                  /* dds_binarypropertyseq_t binary_properties    */
      XbPROP, XS, XO,      /* dds_binaryproperty_t                       */
    XSTOP,
  XSTOP,
  XSTOP                /* end                                            */
};


static void
alias_simple_sequence(ddsi_octetseq_t *dst, const ddsi_octetseq_t *src, size_t elem_size)
{
  dst->length = src->length;
  if (src->length > 0)
  {
    /* Even when aliased, sequence buffers are not shared. */
    dst->value = ddsrt_memdup(src->value, src->length * elem_size);
  }
}

static void
alias_dataholder(nn_dataholder_t *dst, const nn_dataholder_t *src)
{
  dst->class_id = src->class_id;
  alias_simple_sequence((ddsi_octetseq_t*)&dst->properties,
                        (const ddsi_octetseq_t*)&src->properties,
                        sizeof(dds_property_t));
  alias_simple_sequence((ddsi_octetseq_t*)&dst->binary_properties,
                        (const ddsi_octetseq_t*)&src->binary_properties,
                        sizeof(dds_binaryproperty_t));
}

static void
alias_dataholderseq(nn_dataholderseq_t *dst, const nn_dataholderseq_t *src)
{
  dst->n = src->n;
  if (src->n > 0)
  {
    /* Even when aliased, sequence buffers are not shared. */
    dst->tags = ddsrt_malloc(src->n * sizeof(nn_dataholder_t));
    for (uint32_t i = 0; i < src->n; i++)
    {
      alias_dataholder(&(dst->tags[i]), &(src->tags[i]));
    }
  }
}

void
nn_participant_generic_message_init(
   nn_participant_generic_message_t *msg,
   const ddsi_guid_t *wrguid,
   int64_t wrseq,
   const ddsi_guid_t *dstpguid,
   const ddsi_guid_t *dsteguid,
   const ddsi_guid_t *srceguid,
   const char *classid,
   const nn_dataholderseq_t *mdata,
   const nn_message_identity_t *rmid)
{
  assert(msg);
  assert(wrguid);
  assert(classid);

  memset(msg, 0, sizeof(*msg));

  msg->message_identity.source_guid = *wrguid;
  msg->message_identity.sequence_number = wrseq;

  if (rmid)
  {
    msg->related_message_identity.source_guid = rmid->source_guid;
    msg->related_message_identity.sequence_number = rmid->sequence_number;
  }

  if (dstpguid)
    msg->destination_participant_guid = *dstpguid;

  if (dsteguid)
    msg->destination_endpoint_guid = *dsteguid;

  if (srceguid)
    msg->source_endpoint_guid = *srceguid;

  msg->message_class_id = classid;

  if (mdata)
    alias_dataholderseq(&msg->message_data, mdata);
}

void
nn_participant_generic_message_deinit(
   nn_participant_generic_message_t *msg)
{
  assert(msg);
  plist_fini_generic(msg, pserop_participant_generic_message, true);
}

dds_return_t
nn_participant_generic_message_serialize(
   const nn_participant_generic_message_t *msg,
   unsigned char **data,
   size_t *len)
{
  return plist_ser_generic ((void**)data, len, (void*)msg, pserop_participant_generic_message);
}

dds_return_t
nn_participant_generic_message_deseralize(
   nn_participant_generic_message_t *msg,
   const unsigned char *data,
   size_t len,
   bool bswap)
{
  assert(sizeof(nn_participant_generic_message_t) == plist_memsize_generic(pserop_participant_generic_message));
  return plist_deser_generic (msg, data, len, bswap, pserop_participant_generic_message);
}

int
write_crypto_exchange_message(
   const struct participant *pp,
   const ddsi_guid_t *dst_pguid,
   const ddsi_guid_t *src_eguid,
   const ddsi_guid_t *dst_eguid,
   const char *classid,
   const nn_dataholderseq_t *tokens)
{
  struct q_globals * const gv = pp->e.gv;
  struct nn_participant_generic_message pmg;
  struct ddsi_tkmap_instance *tk;
  struct ddsi_serdata *serdata;
  struct proxy_reader *prd;
  ddsi_guid_t prd_guid;
  unsigned char *data;
  size_t len;
  struct writer *wr;
  seqno_t seq;
  int r;

  if ((wr = get_builtin_writer (pp, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER)) == NULL)
  {
    GVLOG (DDS_LC_DISCOVERY, "write_crypto_exchange_message("PGUIDFMT") - builtin volatile secure writer not found\n", PGUID (pp->e.guid));
    return -1;
  }

  prd_guid.prefix = dst_pguid->prefix;
  prd_guid.entityid.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER;
  if ((prd = entidx_lookup_proxy_reader_guid (gv->entity_index, &prd_guid)) == NULL)
  {
    return -1;
  }

  GVLOG (DDS_LC_DISCOVERY, "send crypto tokens("PGUIDFMT" --> "PGUIDFMT")\n", PGUID (wr->e.guid), PGUID (prd_guid));

  ddsrt_mutex_lock (&wr->e.lock);
  seq = ++wr->seq;

  /* Get serialized message. */
  nn_participant_generic_message_init(&pmg, &wr->e.guid, seq, dst_pguid, dst_eguid, src_eguid, classid, tokens, NULL);
  nn_participant_generic_message_serialize(&pmg, &data, &len);

  /* Get the key value. */
  ddsrt_md5_state_t md5st;
  ddsrt_md5_byte_t digest[16];
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (const ddsrt_md5_byte_t *)data, sizeof (nn_message_identity_t));
  ddsrt_md5_finish (&md5st, digest);

  /* Write the sample. */
  struct ddsi_rawcdr_sample raw = {
    .blob = data,
    .size = len,
    .key = digest,
    .keysize = 16
  };
  serdata = ddsi_serdata_from_sample (gv->rawcdr_topic, SDK_DATA, &raw);
  tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, serdata);
  r = write_sample_p2p_wrlock_held(wr, seq, NULL, serdata, tk, prd);
  ddsi_tkmap_instance_unref (gv->m_tkmap, tk);
  ddsi_serdata_unref (serdata);

  nn_participant_generic_message_deinit(&pmg);

  ddsrt_mutex_unlock (&wr->e.lock);

  return r;
}

int volatile_secure_data_filter(struct writer *wr, struct proxy_reader *prd, struct ddsi_serdata *serdata)
{
  static const size_t guid_offset = offsetof(nn_participant_generic_message_t, destination_participant_guid);
  ddsrt_iovec_t guid_ref = { .iov_len=0, .iov_base=NULL };
  ddsi_guid_t *msg_guid;
  ddsi_guid_t pp_guid;
  int pass;

  DDSRT_UNUSED_ARG(wr);

  assert(wr);
  assert(prd);
  assert(serdata);

  (void)ddsi_serdata_to_ser_ref(serdata, guid_offset, sizeof(ddsi_guid_t), &guid_ref);
  assert(guid_ref.iov_len == sizeof(ddsi_guid_t));
  assert(guid_ref.iov_base);
  msg_guid = (ddsi_guid_t*)guid_ref.iov_base;

  pass = is_null_guid(msg_guid);
  if (!pass)
  {
    pp_guid = nn_hton_guid(prd->c.proxypp->e.guid);
    pass = guid_eq(msg_guid, &pp_guid);
  }

  ddsi_serdata_to_ser_unref(serdata, &guid_ref);

  return pass;
}
