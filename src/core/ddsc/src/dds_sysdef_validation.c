// Copyright(c) 2024 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "string.h"
#include "dds/ddsrt/heap.h"
#include "dds__qos.h"
#include "dds__sysdef_validation.h"
#include "dds__sysdef_parser.h"

#define CHECK_DUPLICATE(lib,element_type,element,element_attr,element_descr) \
  do { \
    for (const struct element_type *e2 = lib; e2 != NULL; e2 = (struct element_type *) e2->xmlnode.next) \
    { \
      if (element != e2 && element->element_attr != NULL && e2->element_attr != NULL && strcmp (element->element_attr, e2->element_attr) == 0) { \
        SYSDEF_ERROR ("Duplicate %s '%s'\n", element_descr, element->element_attr); \
        goto failed; \
      } \
    } \
  } while (0)

#define CHECK_NULL_ATTR(lib,element_type,element,element_attr,element_descr) \
  do { \
    if (element->element_attr == NULL) { \
      SYSDEF_ERROR ("%s Attribute "#element_attr" is 'NULL'\n", element_descr); \
      goto failed; \
    } \
  } while (0)

static void free_partitions (uint32_t n_partitions, char **partitions)
{
  for (uint32_t i = 0; i < n_partitions; i++)
    ddsrt_free (partitions[i]);
  ddsrt_free (partitions);
}

static int is_wildcard_partition (const char *str)
{
  return strchr (str, '*') || strchr (str, '?');
}

static dds_return_t validate_qos (dds_qos_t *qos, const char *qos_location)
{
  // History
  dds_history_kind_t history_kind;
  int32_t history_depth;
  if (dds_qget_history (qos, &history_kind, &history_depth) && (history_kind != DDS_HISTORY_KEEP_LAST || history_depth < 0))
  {
    SYSDEF_ERROR ("Unsupported history kind or depth (%s)\n", qos_location);
    goto failed;
  }

  // Partition
  uint32_t n_partitions;
  char **partitions;
  if (dds_qget_partition (qos, &n_partitions, &partitions))
  {
    for (uint32_t i = 0; i < n_partitions; i++)
    {
      if (is_wildcard_partition (partitions[i]))
      {
        SYSDEF_ERROR ("Wildcards in partition name not supported (%s)\n", qos_location);
        free_partitions (n_partitions, partitions);
        goto failed;
      }
    }
    free_partitions (n_partitions, partitions);
  }
  return DDS_RETCODE_OK;

failed:
  return DDS_RETCODE_BAD_PARAMETER;
}

dds_return_t dds_validate_qos_lib (const struct dds_sysdef_system *sysdef, uint64_t qos_mask)
{
  for (const struct dds_sysdef_qos_lib *qos_lib = sysdef->qos_libs; qos_lib != NULL; qos_lib = (struct dds_sysdef_qos_lib *) qos_lib->xmlnode.next)
  {
    CHECK_NULL_ATTR(sysdef->qos_libs, dds_sysdef_qos_lib, qos_lib, name, "QoS library");
    CHECK_DUPLICATE(sysdef->qos_libs, dds_sysdef_qos_lib, qos_lib, name, "QoS library");
    for (const struct dds_sysdef_qos_profile *qos_profile = qos_lib->qos_profiles; qos_profile != NULL; qos_profile = (struct dds_sysdef_qos_profile *) qos_profile->xmlnode.next)
    {
      CHECK_NULL_ATTR(qos_lib->qos_profiles, dds_sysdef_qos_profile, qos_profile, name, "QoS profile");
      CHECK_DUPLICATE(qos_lib->qos_profiles, dds_sysdef_qos_profile, qos_profile, name, "QoS profile");
      for (const struct dds_sysdef_qos *qos = qos_profile->qos; qos != NULL; qos = (struct dds_sysdef_qos *) qos->xmlnode.next)
      {
        CHECK_DUPLICATE(qos_profile->qos, dds_sysdef_qos, qos, name, "QoS");

        uint64_t mask = ~(uint64_t)0U;
        const char *kind;
        switch (qos->kind)
        {
          case DDS_SYSDEF_TOPIC_QOS:
            mask = DDS_TOPIC_QOS_MASK & qos_mask;
            kind = "topic";
            break;
          case DDS_SYSDEF_READER_QOS:
            mask = DDS_READER_QOS_MASK & qos_mask;
            kind = "reader";
            break;
          case DDS_SYSDEF_WRITER_QOS:
            mask = DDS_WRITER_QOS_MASK & qos_mask;
            kind = "writer";
            break;
          case DDS_SYSDEF_SUBSCRIBER_QOS:
            mask = DDS_SUBSCRIBER_QOS_MASK & qos_mask;
            kind = "subscriber";
            break;
          case DDS_SYSDEF_PUBLISHER_QOS:
            mask = DDS_PUBLISHER_QOS_MASK & qos_mask;
            kind = "publisher";
            break;
          case DDS_SYSDEF_PARTICIPANT_QOS:
            mask = DDS_PARTICIPANT_QOS_MASK & qos_mask;
            kind = "participant";
            break;
        }

        // Unsupported policies
        if (qos->qos->present & ~mask)
        {
          SYSDEF_ERROR ("Unsupported policy, non-allowed mask: %08" PRIx64 " (%s::%s, %s QoS%s%s)\n", qos->qos->present & ~mask, qos_lib->name, qos_profile->name, kind, (qos->name != NULL ? " " : ""), (qos->name != NULL ? qos->name : ""));
          goto failed;
        }

        if (validate_qos (qos->qos, qos_profile->name) != DDS_RETCODE_OK)
          goto failed;
      }
    }
  }
  return DDS_RETCODE_OK;

failed:
  return DDS_RETCODE_BAD_PARAMETER;
}
