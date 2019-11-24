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
#ifdef DDSI_INCLUDE_SECURITY

#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/avl.h"

#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_plist.h"
#include "dds/ddsi/ddsi_sertopic.h"
#include "dds/ddsi/ddsi_security_msg.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"

#include "dds/ddsi/q_bswap.h"

struct guid_pair {
  ddsi_guid_t guid1;
  ddsi_guid_t guid2;
};

struct pending_tokens {
  ddsrt_avl_node_t avlnode;
  struct {
    ddsi_guid_t src;
    ddsi_guid_t dst;
  } guids;
  DDS_Security_ParticipantCryptoTokenSeq tokens;
};

struct proxypp_pp_match {
  ddsrt_avl_node_t avlnode;
  DDS_Security_IdentityHandle participant_identity;
  DDS_Security_PermissionsHandle permissions_handle;
  DDS_Security_SharedSecretHandle shared_secret;
  ddsi_guid_t participant_guid;
  bool tokens_available;
};

struct participant_sec_attributes {
  DDS_Security_ParticipantSecurityAttributes attr;
  DDS_Security_ParticipantCryptoHandle crypto_handle;
  ddsrt_avl_tree_t pending_tokens;
  bool plugin_attr;
};

struct proxy_participant_sec_attributes {
  dds_security_access_control *access_control;
  DDS_Security_ParticipantCryptoHandle crypto_handle;
  ddsrt_avl_tree_t local_participants;
};

/*
 * With writer/proxyreader match:
 *      wr_guid - Writer GUID
 *      rd_guid - Proxy reader GUID
 *      wr_crypto_handle - sec_attributes_writer::crypto_handle
 *      rd_crypto_handle - result of register_matched_remote_datareader()
 * With reader/proxywriter match:
 *      wr_guid - Proxy writer GUID
 *      rd_guid - Reader GUID
 *      wr_crypto_handle - result of register_matched_remote_datawriter()
 *      rd_crypto_handle - sec_attributes_reader::crypto_handle
 */
struct reader_writer_match {
  ddsrt_avl_node_t avlnode;
  uint32_t ref_cnt;
  struct {
    ddsi_guid_t wr_guid;
    ddsi_guid_t rd_guid;
  } guids;
  DDS_Security_DatawriterCryptoHandle wr_crypto_handle;
  DDS_Security_DatareaderCryptoHandle rd_crypto_handle;
  bool has_tokens;
};

struct writer_sec_attributes {
  DDS_Security_EndpointSecurityAttributes attr;
  DDS_Security_DatawriterCryptoHandle crypto_handle;
  ddsrt_avl_tree_t wr_prd_matches;
  bool plugin_attr;
};

struct reader_sec_attributes {
  DDS_Security_EndpointSecurityAttributes attr;
  DDS_Security_DatareaderCryptoHandle crypto_handle;
  ddsrt_avl_tree_t rd_pwr_matches;
  bool plugin_attr;
};

static int compare_identity_handle (const void *va, const void *vb);
static int compare_guid_pair(const void *va, const void *vb);

const ddsrt_avl_treedef_t proxypp_pp_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct proxypp_pp_match, avlnode), offsetof (struct proxypp_pp_match, participant_identity), compare_identity_handle, 0);
const ddsrt_avl_treedef_t rd_wr_match_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct reader_writer_match, avlnode), offsetof (struct reader_writer_match, guids), compare_guid_pair, 0);
const ddsrt_avl_treedef_t pending_tokens_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct pending_tokens, avlnode), offsetof (struct pending_tokens, guids), compare_guid_pair, 0);

static int compare_identity_handle (const void *va, const void *vb)
{
  return *((const DDS_Security_IdentityHandle *)va) == *((const DDS_Security_IdentityHandle *)vb);
}

static int guid_eq (const ddsi_guid_t *guid1, const ddsi_guid_t *guid2)
{
  return memcmp (guid1, guid2, sizeof (ddsi_guid_t));
}

static int compare_guid_pair(const void *va, const void *vb)
{
  const struct guid_pair *na = va;
  const struct guid_pair *nb = vb;

  return (guid_eq(&na->guid1, &nb->guid1) && guid_eq(&na->guid2, &nb->guid2));
}

static void
security_exception_clear(
    DDS_Security_SecurityException *exception)
{
  if (exception->message) {
    ddsrt_free(exception->message);
    exception->message = NULL;
  }
}

static void
q_report_security_exception(
    struct q_globals *gv, const char *msg, DDS_Security_SecurityException *exception)
{
  GVERROR("%s: %d:%s", msg, exception->code, exception->message ? exception->message : "Unknown error");
  security_exception_clear(exception);
}


static int
q_omg_security_get_plugins(
    struct participant *pp,
    dds_security_authentication **authentication,
    dds_security_access_control **access_control,
    dds_security_cryptography **crypto)
{
  DDSRT_UNUSED_ARG(pp);

  if (authentication)
    *authentication = NULL;
  if (access_control)
    *access_control = NULL;
  if (crypto)
    *crypto = NULL;
  return 0;
}

static void
g_omg_shallow_copy_StringSeq(
    DDS_Security_StringSeq *dst,
    const ddsi_stringseq_t *src)
{
  unsigned i;
  assert(dst);
  assert(src);

  dst->_length  = src->n;
  dst->_maximum = src->n;
  dst->_buffer  = NULL;
  if (src->n > 0)
  {
    dst->_buffer = ddsrt_malloc(src->n * sizeof(DDS_Security_string));
    for (i = 0; i < src->n; i++)
      dst->_buffer[i] = src->strs[i];
  }
}

static void
g_omg_shallow_free_StringSeq(
    DDS_Security_StringSeq *obj)
{
  if (obj)
    ddsrt_free(obj->_buffer);
}

static void
q_omg_copy_PropertySeq(
    DDS_Security_PropertySeq *dst,
    const dds_propertyseq_t *src)
{
  uint32_t i;

  if (src)
  {
    dst->_length = dst->_maximum = src->n;
    if (src->n > 0)
      dst->_buffer = DDS_Security_PropertySeq_allocbuf(src->n);
    else
      dst->_buffer = NULL;

    for (i = 0; i < src->n; i++)
    {
      dst->_buffer[i].name =  src->props->name ? ddsrt_strdup(src->props->name) : ddsrt_strdup("");
      dst->_buffer[i].value = src->props->value ? ddsrt_strdup(src->props->value) : ddsrt_strdup("");
    }
  }
  else
    memset(dst, 0, sizeof(*dst));
}

static void
q_omg_shallow_copy_PropertySeq(
   DDS_Security_PropertySeq *dst,
   const dds_propertyseq_t *src)
{
  unsigned i;
  assert(dst);
  assert(src);

  dst->_length  = src->n;
  dst->_maximum = src->n;
  dst->_buffer  = NULL;

  if (src->n > 0)
  {
    dst->_buffer = ddsrt_malloc(src->n * sizeof(DDS_Security_Property_t));
    for (i = 0; i < src->n; i++)
    {
      dst->_buffer[i].name      = src->props[i].name;
      dst->_buffer[i].value     = src->props[i].value;
      dst->_buffer[i].propagate = src->props[i].propagate;
    }
  }
}

static void
q_omg_shallow_free_PropertySeq(
    DDS_Security_PropertySeq *obj)
{
  assert(obj);
  ddsrt_free(obj->_buffer);
  obj->_buffer = NULL;
}

static void
q_omg_shallow_copy_BinaryPropertySeq(
    DDS_Security_BinaryPropertySeq *dst,
    const dds_binarypropertyseq_t *src)
{
  unsigned i;
  assert(dst);
  assert(src);

  dst->_length  = src->n;
  dst->_maximum = src->n;
  dst->_buffer  = NULL;

  if (src->n > 0)
  {
    dst->_buffer = ddsrt_malloc(src->n * sizeof(DDS_Security_BinaryProperty_t));
    for (i = 0; i < src->n; i++)
    {
      dst->_buffer[i].name           = src->props[i].name;
      dst->_buffer[i].value._length  = src->props[i].value.length;
      dst->_buffer[i].value._maximum = src->props[i].value.length;
      dst->_buffer[i].value._buffer  = src->props[i].value.value;
      dst->_buffer[i].propagate      = src->props[i].propagate;
    }
  }
}

static void
q_omg_shallow_free_BinaryPropertySeq(
    DDS_Security_BinaryPropertySeq *obj)
{
  assert(obj);
  ddsrt_free(obj->_buffer);
  obj->_buffer = NULL;
}

static void
q_omg_shallow_copy_PropertyQosPolicy(
    DDS_Security_PropertyQosPolicy *dst,
    const dds_property_qospolicy_t *src)
{
    assert(dst);
    assert(src);
    q_omg_shallow_copy_PropertySeq(&(dst->value), &(src->value));
    q_omg_shallow_copy_BinaryPropertySeq(&(dst->binary_value), &(src->binary_value));
}

static void
q_omg_shallow_copy_security_qos(
    DDS_Security_Qos *dst,
    const struct dds_qos *src)
{
  assert(src);
  assert(dst);

  /* DataTags not supported yet. */
  memset(&(dst->data_tags), 0, sizeof(DDS_Security_DataTagQosPolicy));

  if (src->present & QP_PROPERTY_LIST)
    q_omg_shallow_copy_PropertyQosPolicy(&(dst->property), &(src->property));
  else
    memset(&(dst->property), 0, sizeof(DDS_Security_PropertyQosPolicy));
}

static void
q_omg_shallow_free_PropertyQosPolicy(
    DDS_Security_PropertyQosPolicy *obj)
{
  q_omg_shallow_free_PropertySeq(&(obj->value));
  q_omg_shallow_free_BinaryPropertySeq(&(obj->binary_value));
}

static void
q_omg_shallow_free_security_qos(
    DDS_Security_Qos *obj)
{
  q_omg_shallow_free_PropertyQosPolicy(&(obj->property));
}

static void
q_omg_security_dataholder_copyin(
    nn_dataholder_t *dh,
    const DDS_Security_DataHolder *holder)
{
  uint32_t i;

  dh->class_id = holder->class_id ? ddsrt_strdup(holder->class_id) : NULL;
  dh->properties.n = holder->properties._length;
  dh->properties.props = dh->properties.n ? ddsrt_malloc(dh->properties.n * sizeof(dds_property_t)) : NULL;
  for (i = 0; i < dh->properties.n; i++)
  {
    DDS_Security_Property_t *prop = &(holder->properties._buffer[i]);
    dh->properties.props[i].name = prop->name ? ddsrt_strdup(prop->name) : NULL;
    dh->properties.props[i].value = prop->value ? ddsrt_strdup(prop->value) : NULL;
    dh->properties.props[i].propagate = prop->propagate;
  }
  dh->binary_properties.n = holder->binary_properties._length;
  dh->binary_properties.props = dh->binary_properties.n ? ddsrt_malloc(dh->binary_properties.n * sizeof(dds_binaryproperty_t)) : NULL;
  for (i = 0; i < dh->binary_properties.n; i++)
  {
    DDS_Security_BinaryProperty_t *prop = &(holder->binary_properties._buffer[i]);
    dh->binary_properties.props[i].name = prop->name ? ddsrt_strdup(prop->name) : NULL;
    dh->binary_properties.props[i].value.length = prop->value._length;
    if (dh->binary_properties.props[i].value.length)
    {
      dh->binary_properties.props[i].value.value = ddsrt_malloc(prop->value._length);
      memcpy(dh->binary_properties.props[i].value.value, prop->value._buffer, prop->value._length);
    }
    else
    {
      dh->binary_properties.props[i].value.value = NULL;
    }
    dh->binary_properties.props[i].propagate = prop->propagate;
  }
}

#if 0
static void
q_omg_security_dataholder_copyout(
    DDS_Security_DataHolder *holder,
    const nn_dataholder_t *dh)
{
  uint32_t i;

  holder->class_id = dh->class_id ? ddsrt_strdup(dh->class_id) : NULL;
  holder->properties._length = holder->properties._maximum = dh->properties.n;
  holder->properties._buffer = dh->properties.n ? DDS_Security_PropertySeq_allocbuf(dh->properties.n) : NULL;
  for (i = 0; i < dh->properties.n; i++)
  {
    dds_property_t *props = &(dh->properties.props[i]);
    holder->properties._buffer[i].name = props->name ? ddsrt_strdup(props->name) : NULL;
    holder->properties._buffer[i].value = props->value ? ddsrt_strdup(props->value) : NULL;
    holder->properties._buffer[i].propagate = props->propagate;
  }
  holder->binary_properties._length = holder->binary_properties._maximum = dh->binary_properties.n;
  holder->binary_properties._buffer = dh->binary_properties.n ? DDS_Security_BinaryPropertySeq_allocbuf(dh->properties.n) : NULL;
  for (i = 0; i < dh->binary_properties.n; i++)
  {
    dds_binaryproperty_t *props = &(dh->binary_properties.props[i]);
    holder->binary_properties._buffer[i].name = props->name ? ddsrt_strdup(props->name) : NULL;
    holder->binary_properties._buffer[i].value._length = holder->binary_properties._buffer[i].value._maximum = props->value.length;
    if (props->value.length)
    {
      holder->binary_properties._buffer[i].value._buffer = ddsrt_malloc(props->value.length);
      memcpy(holder->binary_properties._buffer[i].value._buffer, props->value.value, props->value.length);
    }
    else
    {
      holder->binary_properties._buffer[i].value._buffer= NULL;
    }
    holder->binary_properties._buffer[i].propagate = props->propagate;
  }
}
#endif

static void
q_omg_shallow_copy_DataHolder(
    DDS_Security_DataHolder *dst,
    const nn_dataholder_t *src)
{
    assert(dst);
    assert(src);
    dst->class_id = src->class_id;
    q_omg_shallow_copy_PropertySeq(&(dst->properties), &(src->properties));
    q_omg_shallow_copy_BinaryPropertySeq(&(dst->binary_properties), &(src->binary_properties));
}

static void
q_omg_shallow_free_DataHolder(
    DDS_Security_DataHolder *obj)
{
    q_omg_shallow_free_PropertySeq(&(obj->properties));
    q_omg_shallow_free_BinaryPropertySeq(&(obj->binary_properties));
}

static void
q_omg_shallow_copy_ParticipantBuiltinTopicDataSecure(
    DDS_Security_ParticipantBuiltinTopicDataSecure *dst,
    const ddsi_guid_t *guid,
    const nn_plist_t *plist)
{
    assert(dst);
    assert(guid);
    assert(plist);

    memset(dst, 0, sizeof(DDS_Security_ParticipantBuiltinTopicDataSecure));

    /* The participant guid is the key. */
    dst->key[0] = guid->prefix.u[0];
    dst->key[1] = guid->prefix.u[1];
    dst->key[2] = guid->prefix.u[2];

    /* Copy the DDS_Security_OctetSeq content (length, pointer, etc), not the buffer content. */
    if (plist->qos.present & QP_USER_DATA) {
        memcpy(&(dst->user_data.value), &(plist->qos.user_data.value), sizeof(DDS_Security_OctetSeq));
    }

    /* Tokens are actually DataHolders. */
    if (plist->present & PP_IDENTITY_TOKEN) {
        q_omg_shallow_copy_DataHolder(&(dst->identity_token), &(plist->identity_token));
    }
    if (plist->present & PP_PERMISSIONS_TOKEN) {
        q_omg_shallow_copy_DataHolder(&(dst->permissions_token), &(plist->permissions_token));
    }
    if (plist->present & PP_IDENTITY_STATUS_TOKEN) {
        q_omg_shallow_copy_DataHolder(&(dst->identity_status_token), &(plist->identity_status_token));
    }

    if (plist->qos.present & QP_PROPERTY_LIST) {
        q_omg_shallow_copy_PropertyQosPolicy(&(dst->property), &(plist->qos.property));
    }

    if (plist->present & PP_PARTICIPANT_SECURITY_INFO) {
        dst->security_info.participant_security_attributes = plist->participant_security_info.security_attributes;
        dst->security_info.plugin_participant_security_attributes = plist->participant_security_info.plugin_security_attributes;
    }
}

static void
q_omg_shallow_free_ParticipantBuiltinTopicDataSecure(
    DDS_Security_ParticipantBuiltinTopicDataSecure *obj)
{
    assert(obj);
    q_omg_shallow_free_DataHolder(&(obj->identity_token));
    q_omg_shallow_free_DataHolder(&(obj->permissions_token));
    q_omg_shallow_free_DataHolder(&(obj->identity_status_token));
    q_omg_shallow_free_PropertyQosPolicy(&(obj->property));
}



static void
pending_tokens_list_add(
    ddsrt_avl_tree_t *tree,
    const ddsi_guid_t *src,
    const ddsi_guid_t *dst,
    const DDS_Security_ParticipantCryptoTokenSeq *tokens)
{
  struct pending_tokens *item;

  item = ddsrt_malloc(sizeof(*item));

  item->guids.src = *src;
  item->guids.dst = *dst;
  DDS_Security_ParticipantCryptoTokenSeq_copy(&item->tokens, tokens);

//  print_tokens(&item->tokens, "pending_tokens_list_add");

  ddsrt_avl_insert(&pending_tokens_treedef, tree, item);
}

static void
pending_tokens_free(
    void *arg)
{
  struct pending_tokens *item = arg;
  if (item) {
    DDS_Security_ParticipantCryptoTokenSeq_freebuf(&item->tokens);
    ddsrt_free(item);
  }
}

static const char *
get_builtin_topic_name(
    ddsi_entityid_t id)
{
  switch (id.u) {
  case NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER:
  case NN_ENTITYID_SEDP_BUILTIN_TOPIC_READER:
    return "DCPSTopic";
    break;
  case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER:
  case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER:
    return "DCPSPublication";
    break;
  case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER:
  case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER:
    return "DCPSSubscription";
    break;
  case NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
  case NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER:
    return "DCPSParticipant";
    break;
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER:
    return "DCPSParticipantMessage";
    break;
  case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER:
  case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER:
    return "DCPSPublicationsSecure";
    break;
  case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER:
  case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER:
    return "DCPSSubscriptionsSecure";
    break;
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER:
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER:
    return "DCPSParticipantStatelessMessage";
    break;
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER:
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER:
    return "DCPSParticipantMessageSecure";
    break;
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER:
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER:
    return "DCPSParticipantVolatileMessageSecure";
    break;
  case NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER:
  case NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER:
    return "DCPSParticipantsSecure";
    break;
  case NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_WRITER:
  case NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_READER:
    return "CMParticipant";
    break;
  case NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_WRITER:
  case NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_READER:
    return "CMPublisher";
    break;
  case NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_WRITER:
  case NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_READER:
    return "CMSubscriber";
    break;
  default:
    return "(null)";
    break;
  }

  return NULL;
}

static const char *
get_reader_topic_name(
    struct reader *rd)
{
  if (rd->topic) {
    return rd->topic->name;
  }
  return get_builtin_topic_name(rd->e.guid.entityid);
}

static const char *
get_writer_topic_name(
    struct writer *wr)
{
  if (wr->topic) {
    return wr->topic->name;
  }
  return get_builtin_topic_name(wr->e.guid.entityid);
}




bool
q_omg_participant_is_secure(
  const struct participant *pp)
{
  /* TODO: Register local participant. */
  DDSRT_UNUSED_ARG(pp);
  return false;
}

bool
q_omg_proxy_participant_is_secure(
    const struct proxy_participant *proxypp)
{
  /* TODO: Register remote participant */
  DDSRT_UNUSED_ARG(proxypp);
  return false;
}

static bool
q_omg_writer_is_discovery_protected(
  const struct writer *wr)
{
  /* TODO: Register local writer. */
  DDSRT_UNUSED_ARG(wr);
  return false;
}

static bool
q_omg_reader_is_discovery_protected(
  const struct reader *rd)
{
  /* TODO: Register local reader. */
  DDSRT_UNUSED_ARG(rd);
  return false;
}

bool
q_omg_get_writer_security_info(
  const struct writer *wr,
  nn_security_info_t *info)
{
  assert(wr);
  assert(info);
  /* TODO: Register local writer. */
  DDSRT_UNUSED_ARG(wr);
  info->plugin_security_attributes = 0;
  info->security_attributes = 0;
  return false;
}

bool
q_omg_get_reader_security_info(
  const struct reader *rd,
  nn_security_info_t *info)
{
  assert(rd);
  assert(info);
  /* TODO: Register local reader. */
  DDSRT_UNUSED_ARG(rd);
  info->plugin_security_attributes = 0;
  info->security_attributes = 0;
  return false;
}

void
q_omg_security_init_remote_participant(struct proxy_participant *proxypp)
{
  DDSRT_UNUSED_ARG(proxypp);
}

static bool
q_omg_proxyparticipant_is_authenticated(
  struct proxy_participant *proxypp)
{
  /* TODO: Handshake */
  DDSRT_UNUSED_ARG(proxypp);
  return false;
}

bool
q_omg_participant_allow_unauthenticated(struct participant *pp)
{
  DDSRT_UNUSED_ARG(pp);

  return true;
}

bool
q_omg_security_check_create_participant(
    struct participant *pp,
    uint32_t domain_id)
{
  bool allowed = false;
  DDS_Security_IdentityHandle identity_handle = DDS_SECURITY_HANDLE_NIL;
  DDS_Security_SecurityException exception = {0};
  DDS_Security_ValidationResult_t result = 0;
  dds_security_authentication *authentication = NULL;
  dds_security_access_control *access_control = NULL;
  dds_security_cryptography *crypto = NULL;
  DDS_Security_IdentityToken identity_token;
  DDS_Security_PermissionsToken permissions_token = {0};
  DDS_Security_PermissionsCredentialToken credential_token = {0};
  DDS_Security_Qos par_qos;
  ddsi_guid_t candidate_guid;
  ddsi_guid_t adjusted_guid;
  int r;

  if ((r = q_omg_security_get_plugins(pp, &authentication, &access_control, &crypto)) < 0)
    goto no_plugin;
  else if (r == 0)
    return true;

  assert(authentication);
  assert(access_control);
  assert(crypto);

  /* Validate local identity */
  ETRACE (pp, "validate_local_identity: candidate_guid: "PGUIDFMT, PGUID (pp->e.guid));

  pp->sec_attr = ddsrt_malloc(sizeof(struct participant_sec_attributes));
  memset(pp->sec_attr, 0, sizeof(struct participant_sec_attributes));
  ddsrt_avl_init (&pending_tokens_treedef, &pp->sec_attr->pending_tokens);

  candidate_guid = nn_hton_guid(pp->e.guid);
  q_omg_shallow_copy_security_qos(&par_qos, &(pp->plist->qos));

  result = authentication->validate_local_identity(
      authentication, &identity_handle,
      (DDS_Security_GUID_t *) &adjusted_guid, (DDS_Security_DomainId) domain_id, &par_qos,
      (DDS_Security_GUID_t *) &candidate_guid, &exception);
  if (result != DDS_SECURITY_VALIDATION_OK)
  {
    q_report_security_exception(pp->e.gv, "Error occurred while validating local permission", &exception);
    goto validation_failed;
  }
  pp->e.guid = nn_ntoh_guid(adjusted_guid);

  ETRACE (pp, " adjusted_guid: "PGUIDFMT"", PGUID (pp->e.guid));

  /* Get the identity token and add this to the plist of the participant */
  if (!authentication->get_identity_token(authentication, &identity_token, identity_handle, &exception))
  {
    q_report_security_exception(pp->e.gv, "Error occurred while retrieving the identity token", &exception);
    goto validation_failed;
  }

  assert(identity_token.class_id);
  q_omg_security_dataholder_copyin(&pp->plist->identity_token, &identity_token);
  DDS_Security_DataHolder_deinit(&identity_token);
  pp->plist->present |= PP_IDENTITY_TOKEN;

  q_omg_shallow_free_security_qos(&par_qos);
  q_omg_shallow_copy_security_qos(&par_qos, &(pp->plist->qos));

  /* ask to access control security plugin for create participant permissions related to this identity*/
  allowed = access_control->check_create_participant(access_control, pp->permissions_handle, (DDS_Security_DomainId) domain_id, &par_qos, &exception);
  if (!allowed)
  {
    q_report_security_exception(pp->e.gv, "It is not allowed to create participant", &exception);
    goto not_allowed;
  }

  /* Get the identity token and add this to the plist of the participant */
  if (!access_control->get_permissions_token(access_control, &permissions_token, pp->permissions_handle, &exception))
  {
    q_report_security_exception(pp->e.gv, "Error occurred while retrieving the permissions token", &exception);
    goto not_allowed;
  }

  assert(permissions_token.class_id);
  q_omg_security_dataholder_copyin(&pp->plist->permissions_token, &permissions_token);
  pp->plist->present |= PP_PERMISSIONS_TOKEN;

  if (!access_control->get_permissions_credential_token(access_control, &credential_token, pp->permissions_handle, &exception))
  {
    q_report_security_exception(pp->e.gv, "Error occurred while retrieving the permissions credential token", &exception);
    goto no_credentials;
  }

  if (!authentication->set_permissions_credential_and_token(authentication, pp->local_identity_handle, &credential_token, &permissions_token, &exception))
  {
    q_report_security_exception(pp->e.gv, "Error occurred while setting the permissions credential token", &exception);
    goto no_credentials;
  }

  if (!access_control->get_participant_sec_attributes(access_control, pp->permissions_handle, &pp->sec_attr->attr, &exception))
  {
    q_report_security_exception(pp->e.gv, "Failed to get participant security attributes", &exception);
    goto no_sec_attr;
  }

  pp->sec_attr->plugin_attr = true;
  pp->sec_attr->crypto_handle = crypto->crypto_key_factory->register_local_participant(
            crypto->crypto_key_factory, pp->local_identity_handle, pp->permissions_handle, NULL, &pp->sec_attr->attr, &exception);
  if (!pp->sec_attr->crypto_handle) {
    q_report_security_exception(pp->e.gv, "Failed to register participant with crypto key factory", &exception);
    goto no_crypto;
  }

  allowed = true;

no_crypto:
no_sec_attr:
  if (permissions_token.class_id)
    (void)access_control->return_permissions_token(access_control, &permissions_token, NULL);
  if (credential_token.class_id)
    (void)access_control->return_permissions_credential_token(access_control, &credential_token, NULL);
no_credentials:
  (void)access_control->return_permissions_token(access_control, &permissions_token, NULL);
not_allowed:
validation_failed:
  q_omg_shallow_free_security_qos(&par_qos);
no_plugin:
  return allowed;
}

#if 0
static struct proxypp_pp_match *
proxypp_pp_match_new(
   DDS_Security_IdentityHandle participant_handle,
   DDS_Security_PermissionsHandle permissions_hdl,
   DDS_Security_SharedSecretHandle shared_secret,
   const ddsi_guid_t *participant_guid)
{
  struct proxypp_pp_match *pm;

  pm = ddsrt_malloc(sizeof(*pm));
  pm->participant_identity = participant_handle;
  pm->permissions_handle = permissions_hdl;
  pm->shared_secret = shared_secret;
  pm->participant_guid = *participant_guid;

  return pm;
}
#endif

static void
proxypp_pp_match_free(
    struct q_globals *gv,
    dds_security_access_control *access_control,
    struct proxypp_pp_match *pm)
{
  if (pm->permissions_handle != DDS_SECURITY_HANDLE_NIL) {
    DDS_Security_SecurityException exception = {0};

    if (!access_control->return_permissions_handle(access_control, pm->permissions_handle, &exception))
      q_report_security_exception(gv, "Failed to return permissions handle", &exception);
  }
  ddsrt_free(pm);
}

static void
q_omg_proxypp_pp_unrelate(
    struct proxy_participant *proxypp,
    struct participant *pp)
{
  if (proxypp->sec_attr) {
    struct proxypp_pp_match *pm;

    if ((pm = ddsrt_avl_lookup (&proxypp_pp_treedef, &proxypp->sec_attr->local_participants, &pp->local_identity_handle)) != NULL) {
      ddsrt_avl_delete(&proxypp_pp_treedef, &proxypp->sec_attr->local_participants, pm);
      proxypp_pp_match_free(proxypp->e.gv, proxypp->sec_attr->access_control, pm);
    }
  }
}

static void
remove_participant_from_remote_entities(
    struct participant *pp)
{
  struct proxy_participant *proxypp;
  struct ephash_enum_proxy_participant it;

  ephash_enum_proxy_participant_init(&it, pp->e.gv->guid_hash);
  while ((proxypp = ephash_enum_proxy_participant_next(&it)) != NULL)
  {
    ddsrt_mutex_lock(&proxypp->e.lock);
    q_omg_proxypp_pp_unrelate(proxypp, pp);
    ddsrt_mutex_unlock(&proxypp->e.lock);
  }
  ephash_enum_proxy_participant_fini(&it);
}

void
q_omg_security_deregister_participant(
    struct participant *pp)
{
  DDS_Security_SecurityException exception = {0};
  dds_security_authentication *authentication = NULL;
  dds_security_access_control *access_control = NULL;

  assert(pp);

  if (pp->sec_attr) {
    remove_participant_from_remote_entities(pp);

    (void)q_omg_security_get_plugins(pp, &authentication, &access_control, NULL);
    assert(authentication);
    assert(access_control);

    /* When the participant is deleted the timed event queue may still contain
     * messages from this participant. Therefore the crypto handle should still
     * be available to ensure that the rtps message can be encoded.
     * For this purpose the cleanup of the associated crypto handle is delayed.
     * A callback is scheduled to be called after some delay to cleanup this
     * crypto handle.
     */
#if 0
    if (pp->sec_attr->crypto_handle != DDS_SECURITY_HANDLE_NIL) {
      struct cleanup_participant_crypto_handle_arg *arg = os_malloc (sizeof (*arg));
      arg->handle = pp->sec_attr->crypto_handle;
      arg->guid = pp->e.guid;
      //cleanup_participant_crypto_handle_arg;
      qxev_callback(add_duration_to_mtime (now_mt(), 100 * T_MILLISECOND), cleanup_participant_crypto_handle, arg);
    }
#endif

    if (pp->permissions_handle != DDS_SECURITY_HANDLE_NIL)
    {
      if (!access_control->return_permissions_handle(access_control, pp->permissions_handle, &exception))
        q_report_security_exception(pp->e.gv, "Failed to return permissions handle", &exception);
    }
    if (pp->local_identity_handle != DDS_SECURITY_HANDLE_NIL)
    {
      if (!authentication->return_identity_handle(authentication, pp->local_identity_handle, &exception))
        q_report_security_exception(pp->e.gv, "Failed to return identity handle", &exception);
    }
    if (pp->sec_attr->plugin_attr)
    {
      if (!access_control->return_participant_sec_attributes(access_control, &pp->sec_attr->attr, &exception))
        q_report_security_exception(pp->e.gv, "Failed to return participant security attributes", &exception);
    }

    ddsrt_avl_free (&pending_tokens_treedef, &pp->sec_attr->pending_tokens, pending_tokens_free);
    ddsrt_free(pp->sec_attr);
  }
}

int64_t
q_omg_security_get_local_participant_handle(
    struct participant *pp)
{
  assert(pp);

  if (pp->sec_attr) {
    return pp->sec_attr->crypto_handle;
  }
  return 0;
}

static bool
q_omg_participant_is_access_protected(
    struct participant *pp)
{
  if (pp->sec_attr) {
    return pp->sec_attr->attr.is_access_protected;
  }
  return false;
}

static bool
is_topic_discovery_protected(
    DDS_Security_PermissionsHandle permission_handle,
    dds_security_access_control *access_control,
    const char *topic_name)
{
  DDS_Security_TopicSecurityAttributes attributes = {0};
  DDS_Security_SecurityException exception = {0};

  if (access_control->get_topic_sec_attributes(access_control, permission_handle, topic_name, &attributes, &exception))
    return attributes.is_discovery_protected;
  else
    security_exception_clear(&exception);
  return false;
}

bool
q_omg_security_check_create_topic(
    struct participant *pp,
    uint32_t domain_id,
    const char *topic_name,
    const struct dds_qos *qos)
{
  dds_security_access_control *access_control = NULL;
  DDS_Security_SecurityException exception = {0};
  DDS_Security_Qos topic_qos;
  bool result;

  if (q_omg_security_get_plugins(pp, NULL, &access_control, NULL) == 0)
    return true;
  assert(access_control);

  q_omg_shallow_copy_security_qos(&topic_qos, qos);
  result = access_control->check_create_topic(access_control, pp->permissions_handle, (DDS_Security_DomainId)domain_id, topic_name, &topic_qos, &exception);
  if (!result)
  {
    /*log if the topic discovery is not protected*/
    if (!is_topic_discovery_protected(pp->permissions_handle, access_control, topic_name))
      q_report_security_exception(pp->e.gv, "Local topic permission denied", &exception);
    else
      security_exception_clear(&exception);
  }
  q_omg_shallow_free_security_qos(&topic_qos);

  return result;
}

static struct reader_writer_match *
reader_writer_match_new(
    ddsi_guid_t *wr_guid,
    ddsi_guid_t *rd_guid,
    DDS_Security_DatawriterCryptoHandle *wr_crypto_handle,
    DDS_Security_DatareaderCryptoHandle *rd_crypto_handle)
{
  struct reader_writer_match *m;

  assert(wr_guid);
  assert(rd_guid);
  assert(wr_crypto_handle || rd_crypto_handle);

  m = ddsrt_malloc(sizeof(*m));
  m->guids.wr_guid = *wr_guid;
  m->guids.rd_guid = *rd_guid;
  if (wr_crypto_handle) {
      m->wr_crypto_handle = *wr_crypto_handle;
  } else {
      m->wr_crypto_handle = DDS_SECURITY_HANDLE_NIL;
  }
  if (rd_crypto_handle) {
      m->rd_crypto_handle = *rd_crypto_handle;
  } else {
      m->rd_crypto_handle = DDS_SECURITY_HANDLE_NIL;
  }
  m->ref_cnt = 1;
  m->has_tokens = false;

  return m;
}

static struct reader_writer_match*
reader_writer_match_keep(
    struct reader_writer_match *m)
{
  assert(m);
  m->ref_cnt++;
  return m;
}

static void
reader_writer_match_release(
    void *arg)
{
  struct reader_writer_match *m = arg;
  if (m) {
    m->ref_cnt--;
    if (m->ref_cnt == 0) {
      ddsrt_free(m);
    }
  }
}

static struct writer_sec_attributes *
writer_sec_attributes_new(void)
{
  struct writer_sec_attributes *attr;

  attr = ddsrt_malloc(sizeof(*attr));
  ddsrt_avl_init (&rd_wr_match_treedef, &attr->wr_prd_matches);
  attr->crypto_handle = DDS_SECURITY_HANDLE_NIL;
  attr->plugin_attr = false;
  return attr;
}

static void
writer_sec_attributes_free(
    struct writer_sec_attributes *attr)
{
  if (attr) {
    ddsrt_avl_free (&rd_wr_match_treedef, &attr->wr_prd_matches, reader_writer_match_release);
    ddsrt_free(attr);
  }
}

bool
q_omg_security_check_create_writer(
    struct participant *pp,
    uint32_t domain_id,
    const char *topic_name,
    const struct dds_qos *writer_qos)
{
  dds_security_access_control *access_control = NULL;
  DDS_Security_SecurityException exception = {0};
  DDS_Security_PartitionQosPolicy partitions;
  DDS_Security_Qos security_qos;
  bool result;

  if (q_omg_security_get_plugins(pp, NULL, &access_control, NULL) == 0)
    return true;

  assert(access_control);

  if (writer_qos->present & QP_PARTITION)
    g_omg_shallow_copy_StringSeq(&partitions.name, &(writer_qos->partition));
  else
    memset(&(partitions), 0, sizeof(DDS_Security_PartitionQosPolicy));

  q_omg_shallow_copy_security_qos(&security_qos, writer_qos);

  result = access_control->check_create_datawriter(access_control, pp->permissions_handle, (DDS_Security_DomainId)domain_id, topic_name, &security_qos, &partitions, NULL, &exception);
  if (!result)
  {
    /*log if the topic discovery is not protected*/
    if (!is_topic_discovery_protected( pp->permissions_handle, access_control, topic_name))
      q_report_security_exception(pp->e.gv, "Writer is not permitted", &exception);
    else
      security_exception_clear(&exception);
  }

  q_omg_shallow_free_security_qos(&security_qos);
  g_omg_shallow_free_StringSeq(&partitions.name);

  return result;
}

void
q_omg_security_register_writer(
    struct writer *wr)
{
  struct q_globals *gv;
  DDS_Security_SecurityException exception = {0};
  dds_security_access_control *access_control = NULL;
  dds_security_cryptography *crypto = NULL;
  DDS_Security_PartitionQosPolicy partitions;
  DDS_Security_PropertySeq properties;
  struct participant *pp = NULL;
  const char *topic_name;

  assert(wr);

  pp = wr->c.pp;
  gv = pp->e.gv;

  if (q_omg_security_get_plugins(pp, NULL, &access_control, &crypto) == 0)
     return;

  assert(access_control);
  assert(crypto);

  if (!pp->sec_attr)
  {
    GVERROR("Failed to register writer "PGUIDFMT": No secure participant.", PGUID(wr->e.guid));
    return;
  }

  if (wr->xqos->present & QP_PARTITION)
    g_omg_shallow_copy_StringSeq(&partitions.name, &(wr->xqos->partition));
  else
    memset(&(partitions), 0, sizeof(DDS_Security_PartitionQosPolicy));

  wr->sec_attr = writer_sec_attributes_new();
  topic_name = get_writer_topic_name(wr);
  if (!access_control->get_datawriter_sec_attributes(access_control, pp->permissions_handle, topic_name, &partitions, NULL, &wr->sec_attr->attr, &exception))
  {
    q_report_security_exception(gv, "Failed to retrieve writer security attributes", &exception);
    goto no_attr;
  }
  wr->sec_attr->plugin_attr = true;

  if (wr->sec_attr->attr.is_payload_protected || wr->sec_attr->attr.is_submessage_protected)
  {
    if (wr->xqos->present & QP_PROPERTY_LIST)
      q_omg_copy_PropertySeq(&properties, &wr->xqos->property.value);
    else
      memset(&properties, 0, sizeof(DDS_Security_PropertySeq));

    wr->sec_attr->crypto_handle = crypto->crypto_key_factory->register_local_datawriter(
        crypto->crypto_key_factory, pp->sec_attr->crypto_handle, &properties, &wr->sec_attr->attr, &exception);
    DDS_Security_PropertySeq_freebuf(&properties);
    if (wr->sec_attr->crypto_handle == DDS_SECURITY_HANDLE_NIL)
    {
      q_report_security_exception(gv, "Failed to register writer with crypto", &exception);
      goto not_registered;
    }
  }

  if (wr->sec_attr->attr.is_key_protected)
    wr->include_keyhash = 1;

not_registered:
no_attr:
  g_omg_shallow_free_StringSeq(&partitions.name);
}

void
q_omg_security_deregister_writer(
    struct writer *wr)
{
  DDS_Security_SecurityException exception = {0};
  dds_security_access_control *access_control = NULL;
  dds_security_cryptography *crypto = NULL;

  assert(wr);

  if (q_omg_security_get_plugins(wr->c.pp, NULL, &access_control, &crypto) < 0)
    return;

  if (wr->sec_attr)
  {
    if (wr->sec_attr->crypto_handle != DDS_SECURITY_HANDLE_NIL)
    {
      if (!crypto->crypto_key_factory->unregister_datawriter(crypto->crypto_key_factory, wr->sec_attr->crypto_handle, &exception))
        q_report_security_exception(wr->e.gv, "Failed to unregister writer with crypto", &exception);
    }
    if (wr->sec_attr->plugin_attr)
    {
      if (!access_control->return_datawriter_sec_attributes(access_control, &wr->sec_attr->attr, &exception))
        q_report_security_exception(wr->e.gv, "Failed to return writer security attributes", &exception);
    }
    writer_sec_attributes_free(wr->sec_attr);
    wr->sec_attr = NULL;
  }
}

static struct reader_sec_attributes *
reader_sec_attributes_new(void) {
  struct reader_sec_attributes *attr;

  attr = ddsrt_malloc(sizeof(*attr));
  ddsrt_avl_init (&rd_wr_match_treedef, &attr->rd_pwr_matches);
  attr->crypto_handle = DDS_SECURITY_HANDLE_NIL;
  attr->plugin_attr = false;

  return attr;
}

static void
reader_sec_attributes_free(
    struct reader_sec_attributes *attr)
{
  if (attr) {
    ddsrt_avl_free (&rd_wr_match_treedef, &attr->rd_pwr_matches, reader_writer_match_release);
    ddsrt_free(attr);
  }
}

bool
q_omg_security_check_create_reader(
    struct participant *pp,
    uint32_t domain_id,
    const char *topic_name,
    const struct dds_qos *reader_qos)
{
  dds_security_access_control *access_control = NULL;
  DDS_Security_SecurityException exception = {0};
  DDS_Security_PartitionQosPolicy partitions;
  DDS_Security_Qos security_qos;
  bool result;

  if (q_omg_security_get_plugins(pp, NULL, &access_control, NULL) == 0)
    return true;

  assert(access_control);

  if (reader_qos->present & QP_PARTITION)
    g_omg_shallow_copy_StringSeq(&partitions.name, &(reader_qos->partition));
  else
    memset(&(partitions), 0, sizeof(DDS_Security_PartitionQosPolicy));

  q_omg_shallow_copy_security_qos(&security_qos, reader_qos);

  result = access_control->check_create_datareader(access_control, pp->permissions_handle, (DDS_Security_DomainId)domain_id, topic_name, &security_qos, &partitions, NULL, &exception);
  if (!result)
  {
    /*log if the topic discovery is not protected*/
    if (!is_topic_discovery_protected( pp->permissions_handle, access_control, topic_name))
      q_report_security_exception(pp->e.gv, "Reader is not permitted", &exception);
    else
      security_exception_clear(&exception);
  }

  q_omg_shallow_free_security_qos(&security_qos);
  g_omg_shallow_free_StringSeq(&partitions.name);

  return result;
}

void
q_omg_security_register_reader(
    struct reader *rd)
{
  struct q_globals *gv;
  DDS_Security_SecurityException exception = {0};
  dds_security_access_control *access_control = NULL;
  dds_security_cryptography *crypto = NULL;
  DDS_Security_PartitionQosPolicy partitions;
  DDS_Security_PropertySeq properties;
  struct participant *pp = NULL;
  const char *topic_name;

  assert(rd);

  pp = rd->c.pp;
  gv = pp->e.gv;

  if (q_omg_security_get_plugins(pp, NULL, &access_control, &crypto) == 0)
    return;
  assert(access_control);
  assert(crypto);

  if (!pp->sec_attr)
  {
    GVERROR("Failed to register reader "PGUIDFMT": No secure participant.", PGUID(rd->e.guid));
    return;
  }

  if (rd->xqos->present & QP_PARTITION)
    g_omg_shallow_copy_StringSeq(&partitions.name, &(rd->xqos->partition));
  else
    memset(&(partitions), 0, sizeof(DDS_Security_PartitionQosPolicy));

  rd->sec_attr = reader_sec_attributes_new();

  topic_name = get_reader_topic_name(rd);
  if (!access_control->get_datareader_sec_attributes(access_control, pp->permissions_handle, topic_name, &partitions, NULL, &rd->sec_attr->attr, &exception))
  {
    q_report_security_exception(gv, "Failed to retrieve reader security attributes", &exception);
    goto no_attr;
  }
  rd->sec_attr->plugin_attr = true;

  if (rd->sec_attr->attr.is_payload_protected || rd->sec_attr->attr.is_submessage_protected)
  {
    if (rd->xqos->present & QP_PROPERTY_LIST)
      q_omg_copy_PropertySeq(&properties, &rd->xqos->property.value);
    else
      memset(&properties, 0, sizeof(DDS_Security_PropertySeq));

    rd->sec_attr->crypto_handle = crypto->crypto_key_factory->register_local_datareader(
        crypto->crypto_key_factory, pp->sec_attr->crypto_handle, &properties, &rd->sec_attr->attr, &exception);
    DDS_Security_PropertySeq_freebuf(&properties);
    if (rd->sec_attr->crypto_handle == DDS_SECURITY_HANDLE_NIL)
    {
      q_report_security_exception(gv, "Failed to register reader with crypto", &exception);
      goto not_registered;
    }
  }

not_registered:
no_attr:
  g_omg_shallow_free_StringSeq(&partitions.name);
}

void
q_omg_security_deregister_reader(
    struct reader *rd)
{
  DDS_Security_SecurityException exception = {0};
  dds_security_access_control *access_control = NULL;
  dds_security_cryptography *crypto = NULL;

  assert(rd);

  if (q_omg_security_get_plugins(rd->c.pp, NULL, &access_control, &crypto) < 0)
    return;

  if (rd->sec_attr)
  {
    if (rd->sec_attr->crypto_handle != DDS_SECURITY_HANDLE_NIL)
    {
      if (!crypto->crypto_key_factory->unregister_datareader(crypto->crypto_key_factory, rd->sec_attr->crypto_handle, &exception))
        q_report_security_exception(rd->e.gv, "Failed to unregister reader with crypto", &exception);
    }
    if (rd->sec_attr->plugin_attr)
    {
      if (!access_control->return_datareader_sec_attributes(access_control, &rd->sec_attr->attr, &exception))
        q_report_security_exception(rd->e.gv, "Failed to return reader security attributes", &exception);
    }
    reader_sec_attributes_free(rd->sec_attr);
    rd->sec_attr = NULL;
  }
}

unsigned
determine_subscription_writer(
  const struct reader *rd)
{
  if (q_omg_reader_is_discovery_protected(rd))
  {
    return NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER;
  }
  return NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER;
}

unsigned
determine_publication_writer(
  const struct writer *wr)
{
  if (q_omg_writer_is_discovery_protected(wr))
  {
    return NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER;
  }
  return NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER;
}







int64_t
q_omg_security_check_remote_participant_permissions(uint32_t domain_id, struct participant *pp, struct proxy_participant *proxypp)
{
  struct q_globals *gv = pp->e.gv;
  DDS_Security_SecurityException exception = {0};
  dds_security_access_control *access_control;
  dds_security_authentication *authentication;
  struct q_handshake *handshake;
  DDS_Security_PermissionsToken permissions_token = {0};
  DDS_Security_AuthenticatedPeerCredentialToken peer_credential_token = {0};
  int64_t permissions_hdl = DDS_SECURITY_HANDLE_NIL;

  if (q_omg_security_get_plugins(pp, &authentication, &access_control, NULL) == 0)
  {
    assert(false);
    return 0;
  }

  ddsrt_mutex_lock(&proxypp->e.lock);

  if (proxypp->plist->present & PP_PERMISSIONS_TOKEN)
      q_omg_shallow_copy_DataHolder(&permissions_token, &proxypp->plist->permissions_token);
  else
      memset(&permissions_token, 0, sizeof(DDS_Security_PermissionsToken));

  handshake = q_hsadmin_find(proxypp->handshake_admin, &pp->e.guid);
  if (!handshake)
  {
      GVERROR("Could not find handshake local participant "PGUIDFMT" and remote participant "PGUIDFMT,
                PGUID(pp->e.guid), PGUID(proxypp->e.guid));
      goto no_handshake;
  }

  if (!authentication->get_authenticated_peer_credential_token(authentication, &peer_credential_token, q_handshake_get_handle(handshake), &exception))
  {
    if (q_omg_participant_is_access_protected(pp))
    {
      GVERROR("Could not authenticate_peer_credential_token for local participant "PGUIDFMT" and remote participant "PGUIDFMT": %s",
          PGUID(pp->e.guid), PGUID(proxypp->e.guid), exception.message ? exception.message : "Unknown error");
    } else {
      /* Failing is allowed due to the non-protection of access. */
      GVWARNING("Could not authenticate_peer_credential_token for local participant "PGUIDFMT" and remote participant "PGUIDFMT": %s",
          PGUID(pp->e.guid), PGUID(proxypp->e.guid), exception.message ? exception.message : "Unknown error");
    }
    security_exception_clear(&exception);
    goto no_credentials;
  }

  permissions_hdl = access_control->validate_remote_permissions(
      access_control, authentication, pp->local_identity_handle, proxypp->remote_identity_handle, &permissions_token, &peer_credential_token, &exception);

  if (permissions_hdl == DDS_SECURITY_HANDLE_NIL) {
    if (q_omg_participant_is_access_protected(pp)) {
      GVWARNING("Could not get remote participant "PGUIDFMT" permissions from plugin: %s",
          PGUID(proxypp->e.guid), exception.message ? exception.message : "Unknown error");
      goto no_permissions;
    }
    else
    {
      /* Failing is allowed due to the non-protection of access. */
      GVWARNING("Could not get remote participant "PGUIDFMT" permissions from plugin: %s",
          PGUID(proxypp->e.guid), exception.message ? exception.message : "Unknown error");
    }
    security_exception_clear(&exception);
  }

  /* Only check remote participant if joining access is protected. */
  if (q_omg_participant_is_access_protected(pp))
  {
      DDS_Security_ParticipantBuiltinTopicDataSecure participant_data;

      q_omg_shallow_copy_ParticipantBuiltinTopicDataSecure(&participant_data, &(proxypp->e.guid), proxypp->plist);
      if (!access_control->check_remote_participant(access_control, permissions_hdl, (DDS_Security_DomainId)domain_id, &participant_data, &exception))
      {
        GVWARNING("Plugin does not allow remote participant "PGUIDFMT": %s",
            PGUID(proxypp->e.guid), exception.message ? exception.message : "Unknown error");
        security_exception_clear(&exception);
        if (!access_control->return_permissions_handle(access_control, permissions_hdl, &exception))
          q_report_security_exception(pp->e.gv, "Failed to return permissions handle", &exception);
        permissions_hdl = DDS_SECURITY_HANDLE_NIL;
      }
      q_omg_shallow_free_ParticipantBuiltinTopicDataSecure(&participant_data);
  }

no_permissions:
  if (!authentication->return_authenticated_peer_credential_token(authentication, &peer_credential_token, &exception))
    q_report_security_exception(pp->e.gv, "Failed to return peer credential token", &exception);
no_credentials:
  q_handshake_release(handshake);
no_handshake:
  q_omg_shallow_free_DataHolder(&permissions_token);
  ddsrt_mutex_unlock(&proxypp->e.lock);
  return permissions_hdl;
}

#if 0
void
q_omg_security_register_remote_participant(struct participant *pp, struct proxy_participant *proxypp, int64_t shared_secret, int64_t proxy_permissions)
{
  os_boolean result = OS_TRUE;
  DDS_Security_SecurityException ex = SECURITY_EXCEPTION_INIT;
  DDS_Security_ParticipantCryptoHandle crypto_handle;
  struct proxypp_pp_match *pm;
  struct pending_tokens *pending;

  if (!config.omg_security_configuration) {
     return;
  }

  assert(pp);
  assert(pp->sec_attr);
  assert(proxypp);
  assert(proxypp->sec_attr);

  TRACE(("register remote participant  "PGUIDFMT" with "PGUIDFMT"\n", PGUID(proxypp->e.guid), PGUID(pp->e.guid)));

  os_mutexLock(&proxypp->e.lock);

  crypto_handle = crypto_context->crypto_key_factory->register_matched_remote_participant(
                      crypto_context->crypto_key_factory, pp->sec_attr->crypto_handle,
                      proxypp->remote_identity_handle, proxy_permissions, shared_secret, &ex);
  if (crypto_handle == DDS_SECURITY_HANDLE_NIL) {
    NN_ERROR3("Failed to register matched remote participant "PGUIDFMT" with participant "PGUIDFMT" : %s",
        PGUID(proxypp->e.guid), PGUID(pp->e.guid), ex.message ? ex.message : "Unknown error");
    TRACE(("\n"));
    security_exception_reset(&ex);
    result = OS_FALSE;
    goto fail_register;
  }

  if (proxypp->sec_attr->crypto_handle == DDS_SECURITY_HANDLE_NIL) {
    proxypp->sec_attr->crypto_handle = crypto_handle;
  } else {
    assert(proxypp->sec_attr->crypto_handle == crypto_handle);
  }

  pm = proxypp_pp_match_new(pp->local_identity_handle, proxy_permissions, shared_secret, &pp->e.guid);
  (void)ut_hhAdd(proxypp->sec_attr->local_participant_hash, pm);

  pending = pending_tokens_list_find(&proxypp->sec_attr->pending_tokens, &proxypp->e.guid, &pp->e.guid);
  if (pending) {
    result = crypto_context->crypto_key_exchange->set_remote_participant_crypto_tokens(
                 crypto_context->crypto_key_exchange, pp->sec_attr->crypto_handle,
                 proxypp->sec_attr->crypto_handle, &pending->tokens, &ex);
    if (result) {
      pm->tokens_available = OS_TRUE;
      if ((pp == gv.privileged_pp) || !proxypp->sec_attr->cached_local_participant) {
        proxypp->sec_attr->cached_local_participant = pp;
      }
      TRACE(("set participant tokens src("PGUIDFMT") to  dst("PGUIDFMT") (by registering remote)\n", PGUID(proxypp->e.guid), PGUID(pp->e.guid)));
    } else {
      NN_ERROR3("Failed to set remote participant crypto tokens "PGUIDFMT" --> "PGUIDFMT" : %s",
          PGUID(proxypp->e.guid), PGUID(pp->e.guid), ex.message ? ex.message : "Unknown error");
      TRACE(("\n"));
      security_exception_reset(&ex);
    }
    pending_tokens_list_remove(&proxypp->sec_attr->pending_tokens, pending);
    pending_tokens_free(pending);
  }

  remote_participant_crypto_handle_list_update();

#if 0
  if (result) {
    result = send_participant_crypto_tokens(pp, proxypp, pp->sec_attr->crypto_handle, proxypp->sec_attr->crypto_handle);
  }
#endif

fail_register:
  os_mutexUnlock(&proxypp->e.lock);
}
#endif

void
q_omg_security_deregister_remote_participant(struct proxy_participant *proxypp)
{
  DDSRT_UNUSED_ARG(proxypp);
}

bool
allow_proxy_participant_deletion(
  struct q_globals * const gv,
  const struct ddsi_guid *guid,
  const ddsi_entityid_t pwr_entityid)
{
  struct proxy_participant *proxypp;

  assert(gv);
  assert(guid);

  /* Always allow deletion from a secure proxy writer. */
  if (pwr_entityid.u == NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER)
    return true;

  /* Not from a secure proxy writer.
   * Only allow deletion when proxy participant is not authenticated. */
  proxypp = ephash_lookup_proxy_participant_guid(gv->guid_hash, guid);
  if (!proxypp)
  {
    GVLOGDISC (" unknown");
    return false;
  }
  return (!q_omg_proxyparticipant_is_authenticated(proxypp));
}

bool
q_omg_is_similar_participant_security_info(struct participant *pp, struct proxy_participant *proxypp)
{
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);

  return true;
}

void
q_omg_security_participant_send_tokens(struct participant *pp, struct proxy_participant *proxypp)
{
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);
}

bool
q_omg_security_match_remote_writer_enabled(struct reader *rd, struct proxy_writer *pwr)
{
  DDSRT_UNUSED_ARG(rd);
  DDSRT_UNUSED_ARG(pwr);

  assert(rd);
  assert(pwr);

  return true;
}

bool
q_omg_security_match_remote_reader_enabled(struct writer *wr, struct proxy_reader *prd)
{
  DDSRT_UNUSED_ARG(wr);
  DDSRT_UNUSED_ARG(prd);

  assert(wr);
  assert(prd);

  return true;
}

bool
q_omg_security_check_remote_writer_permissions(const struct proxy_writer *pwr, uint32_t domain_id, struct participant *pp)
{
  DDSRT_UNUSED_ARG(pwr);
  DDSRT_UNUSED_ARG(domain_id);
  DDSRT_UNUSED_ARG(pp);

  assert(pwr);
  assert(pp);
  assert(pwr->c.proxypp);

  return true;
}

bool
q_omg_security_check_remote_reader_permissions(const struct proxy_reader *prd, uint32_t domain_id, struct participant *pp)
{
  DDSRT_UNUSED_ARG(prd);
  DDSRT_UNUSED_ARG(domain_id);
  DDSRT_UNUSED_ARG(pp);

  assert(prd);
  assert(pp);
  assert(prd->c.proxypp);

  return true;
}

int64_t
q_omg_security_get_remote_participant_handle(struct proxy_participant *proxypp)
{
  DDSRT_UNUSED_ARG(proxypp);
  return 0;
}

#else /* DDSI_INCLUDE_SECURITY */

#include "dds/ddsi/ddsi_security_omg.h"

extern inline bool q_omg_participant_is_secure(UNUSED_ARG(const struct participant *pp));
extern inline bool q_omg_proxy_participant_is_secure(const struct proxy_participant *proxypp);

extern inline unsigned determine_subscription_writer(UNUSED_ARG(const struct reader *rd));
extern inline unsigned determine_publication_writer(UNUSED_ARG(const struct writer *wr));

extern inline bool q_omg_security_match_remote_writer_enabled(UNUSED_ARG(struct reader *rd), UNUSED_ARG(struct proxy_writer *pwr));
extern inline bool q_omg_security_match_remote_reader_enabled(UNUSED_ARG(struct writer *wr), UNUSED_ARG(struct proxy_reader *prd));

extern inline bool q_omg_security_check_remote_writer_permissions(UNUSED_ARG(const struct proxy_writer *pwr), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(struct participant *pp));
extern inline bool q_omg_security_check_remote_reader_permissions(UNUSED_ARG(const struct proxy_reader *prd), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(struct participant *par));

extern inline bool allow_proxy_participant_deletion(
  UNUSED_ARG(struct q_globals * const gv),
  UNUSED_ARG(const struct ddsi_guid *guid),
  UNUSED_ARG(const ddsi_entityid_t pwr_entityid));

extern inline bool q_omg_is_similar_participant_security_info(UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp));

extern inline bool q_omg_participant_allow_unauthenticated(UNUSED_ARG(struct participant *pp));

extern inline bool
q_omg_security_check_create_participant(UNUSED_ARG(struct participant *pp), UNUSED_ARG(uint32_t domain_id));

extern inline void
q_omg_security_deregister_participant(UNUSED_ARG(struct participant *pp));

extern inline bool
q_omg_security_check_create_topic(UNUSED_ARG(struct participant *pp), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(const char *topic_name), UNUSED_ARG(const struct dds_qos *qos));

extern inline int64_t
q_omg_security_get_local_participant_handle(UNUSED_ARG(struct participant *pp);

extern inline bool
q_omg_security_check_create_writer(UNUSED_ARG(struct participant *pp), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(const char *topic_name), UNUSED_ARG(const struct dds_qos *writer_qos));

extern inline void
q_omg_security_register_writer(UNUSED_ARG(struct writer *wr));

extern inline void
q_omg_security_deregister_writer(UNUSED_ARG(struct writer *wr));

extern inline bool
q_omg_security_check_create_reader(UNUSED_ARG(struct participant *pp), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(const char *topic_name), UNUSED_ARG(const struct dds_qos *reader_qos));

extern inline void
q_omg_security_register_reader(UNUSED_ARG(struct reader *rd));

extern inline void
q_omg_security_deregister_reader(UNUSED_ARG(struct reader *rd));

/* initialize the proxy participant security attributes */
extern inline void q_omg_security_init_remote_participant(UNUSED_ARG(struct proxy_participant *proxypp));

/* ask to access control security plugin for the remote participant permissions */
extern inline int64_t q_omg_security_check_remote_participant_permissions(UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp));

extern inline void q_omg_security_register_remote_participant(UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp), UNUSED_ARG(int64_t shared_secret), UNUSED_ARG(int64_t proxy_permissions));

extern inline void q_omg_security_deregister_remote_participant(UNUSED_ARG(struct proxy_participant *proxypp));

extern inline void q_omg_security_participant_send_tokens(UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp));

extern inline int64_t q_omg_security_get_remote_participant_handle(UNUSED_ARG(struct proxy_participant *proxypp));

#endif /* DDSI_INCLUDE_SECURITY */
