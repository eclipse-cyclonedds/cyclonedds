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
#include <assert.h>
#include <string.h>

#include "ddsi/q_misc.h"
#include "ddsi/q_config.h"
#include "ddsi/q_entity.h"
#include "ddsi/q_builtin_topic.h"

#include "dds_builtinTopics.h"

static void generate_user_data (_Out_ DDS_UserDataQosPolicy *a, _In_ const nn_xqos_t *xqos)
{
  if (!(xqos->present & QP_USER_DATA) || (xqos->user_data.length == 0))
  {
    a->value._maximum = 0;
    a->value._length  = 0;
    a->value._buffer  = NULL;
    a->value._release = false;
  } else {
    a->value._maximum = xqos->user_data.length;
    a->value._length  = xqos->user_data.length;
    a->value._buffer  = xqos->user_data.value;
    a->value._release = false;
  }
}

static void
generate_product_data(
        _Out_ DDS_ProductDataQosPolicy *a,
        _In_ const struct entity_common *participant,
        _In_ const nn_plist_t *plist)
{
  /* replicate format generated in v_builtinCreateCMParticipantInfo() */
  static const char product_tag[] = "Product";
  static const char exec_name_tag[] = "ExecName";
  static const char participant_name_tag[] = "ParticipantName";
  static const char process_id_tag[] = "PID";
  static const char node_name_tag[] = "NodeName";
  static const char federation_id_tag[] = "FederationId";
  static const char vendor_id_tag[] = "VendorId";
  static const char service_type_tag[] = "ServiceType";
  const size_t cdata_overhead = 12; /* <![CDATA[ and ]]> */
  const size_t tag_overhead = 5; /* <> and </> */
  char pidstr[11]; /* unsigned 32-bits, so max < 5e9, or 10 chars + terminator */
  char federationidstr[20]; /* max 2 * unsigned 32-bits hex + separator, terminator */
  char vendoridstr[22]; /* max 2 * unsigned 32-bits + seperator, terminator */
  char servicetypestr[11]; /* unsigned 32-bits */
  unsigned servicetype;
  size_t len = 1 + 2*(sizeof(product_tag)-1) + tag_overhead;

  if (plist->present & PP_PRISMTECH_EXEC_NAME)
    len += 2*(sizeof(exec_name_tag)-1) + cdata_overhead + tag_overhead + strlen(plist->exec_name);
  if (plist->present & PP_ENTITY_NAME)
    len += 2*(sizeof(participant_name_tag)-1) + cdata_overhead + tag_overhead + strlen(plist->entity_name);
  if (plist->present & PP_PRISMTECH_PROCESS_ID)
  {
    int n = snprintf (pidstr, sizeof (pidstr), "%u", plist->process_id);
    assert (n > 0 && (size_t) n < sizeof (pidstr));
    len += 2*(sizeof(process_id_tag)-1) + tag_overhead + (size_t) n;
  }
  if (plist->present & PP_PRISMTECH_NODE_NAME)
    len += 2*(sizeof(node_name_tag)-1) + cdata_overhead + tag_overhead + strlen(plist->node_name);

  {
    int n = snprintf (vendoridstr, sizeof (vendoridstr), "%u.%u", plist->vendorid.id[0], plist->vendorid.id[1]);
    assert (n > 0 && (size_t) n < sizeof (vendoridstr));
    len += 2*(sizeof(vendor_id_tag)-1) + tag_overhead + (size_t) n;
  }

  {
    int n;
    if (vendor_is_opensplice (plist->vendorid))
      n = snprintf (federationidstr, sizeof (federationidstr), "%x", participant->guid.prefix.u[0]);
    else
      n = snprintf (federationidstr, sizeof (federationidstr), "%x:%x", participant->guid.prefix.u[0], participant->guid.prefix.u[1]);
    assert (n > 0 && (size_t) n < sizeof (federationidstr));
    len += 2*(sizeof(federation_id_tag)-1) + tag_overhead + (size_t) n;
  }

  if (plist->present & PP_PRISMTECH_SERVICE_TYPE)
    servicetype = plist->service_type;
  else
    servicetype = 0;

  {
    int n = snprintf (servicetypestr, sizeof (servicetypestr), "%u", (unsigned) servicetype);
    assert (n > 0 && (size_t) n < sizeof (servicetypestr));
    len += 2*(sizeof(service_type_tag)-1) + tag_overhead + (size_t) n;
  }

  a->value = os_malloc(len);

  {
    char *p = a->value;
    int n;
    n = snprintf (p, len, "<%s>", product_tag); assert (n >= 0 && (size_t) n < len); p += n; len -= (size_t) n;
    if (plist->present & PP_PRISMTECH_EXEC_NAME)
    {
      n = snprintf (p, len, "<%s><![CDATA[%s]]></%s>", exec_name_tag, plist->exec_name, exec_name_tag);
      assert (n >= 0 && (size_t) n < len);
      p += n; len -= (size_t) n;
    }
    if (plist->present & PP_ENTITY_NAME)
    {
      n = snprintf (p, len, "<%s><![CDATA[%s]]></%s>", participant_name_tag, plist->entity_name, participant_name_tag);
      assert (n >= 0 && (size_t) n < len);
      p += n; len -= (size_t) n;
    }
    if (plist->present & PP_PRISMTECH_PROCESS_ID)
    {
      n = snprintf (p, len, "<%s>%s</%s>", process_id_tag, pidstr, process_id_tag);
      assert (n >= 0 && (size_t) n < len);
      p += n; len -= (size_t) n;
    }
    if (plist->present & PP_PRISMTECH_NODE_NAME)
    {
      n = snprintf (p, len, "<%s><![CDATA[%s]]></%s>", node_name_tag, plist->node_name, node_name_tag);
      assert (n >= 0 && (size_t) n < len);
      p += n; len -= (size_t) n;
    }
    n = snprintf (p, len, "<%s>%s</%s>", federation_id_tag, federationidstr, federation_id_tag);
    assert (n >= 0 && (size_t) n < len);
    p += n; len -= (size_t) n;
    n = snprintf (p, len, "<%s>%s</%s>", vendor_id_tag, vendoridstr, vendor_id_tag);
    assert (n >= 0 && (size_t) n < len);
    p += n; len -= (size_t) n;

    {
      n = snprintf (p, len, "<%s>%s</%s>", service_type_tag, servicetypestr, service_type_tag);
      assert (n >= 0 && (size_t) n < len);
      p += n; len -= (size_t) n;
    }

    n = snprintf (p, len, "</%s>", product_tag);
    assert (n >= 0 && (size_t) n == len-1);
    (void) n;
  }
}

static void generate_key (_Out_ DDS_BuiltinTopicKey_t *a, _In_ const nn_guid_prefix_t *gid)
{
  (*a)[0] = gid->u[0];
  (*a)[1] = gid->u[1];
  (*a)[2] = gid->u[2];
}

void
propagate_builtin_topic_participant(
        _In_ const struct entity_common *participant,
        _In_ const nn_plist_t *plist,
        _In_ nn_wctime_t timestamp,
        _In_ int alive)
{
  DDS_ParticipantBuiltinTopicData data;
  generate_key(&(data.key), &(participant->guid.prefix));
  generate_user_data(&(data.user_data), &(plist->qos));
  forward_builtin_participant(&data, timestamp, alive);
}

void
propagate_builtin_topic_cmparticipant(
        _In_ const struct entity_common *participant,
        _In_ const nn_plist_t *plist,
        _In_ nn_wctime_t timestamp,
        _In_ int alive)
{
  DDS_CMParticipantBuiltinTopicData data;
  generate_key(&(data.key), &(participant->guid.prefix));
  generate_product_data(&(data.product), participant, plist);
  forward_builtin_cmparticipant(&data, timestamp, alive);
  os_free(data.product.value);
}
