/*
 * Copyright(c) 2025 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include "dds_topic_descriptor_serde.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dds/cdr/dds_cdrstream.h"

// Helper function to calculate serialized size
size_t dds_topic_descriptor_serialized_size (const dds_topic_descriptor_t *desc)
{
  size_t size = 0;

  // Fixed fields
  size += sizeof (desc->m_size);
  size += sizeof (desc->m_align);
  size += sizeof (desc->m_flagset);
  size += sizeof (desc->m_nkeys);
  size += sizeof (desc->m_nops);
  size += sizeof (desc->restrict_data_representation);

  // m_typename (length + string)
  size += sizeof (uint32_t);
  if (desc->m_typename)
  {
    size += strlen (desc->m_typename) + 1;
  }

  // m_keys array
  size += desc->m_nkeys * sizeof (dds_key_descriptor_t);
  for (uint32_t i = 0; i < desc->m_nkeys; i++)
  {
    size += sizeof (uint32_t); // name length
    if (desc->m_keys[i].m_name)
    {
      size += strlen (desc->m_keys[i].m_name) + 1;
    }
  }

  // m_ops array
  // m_ops array length is not always m_nops, and it seems the internal implement doesn't
  // really rely on it, but calculate on the run.
  // so we discard the origin m_nops and calculate it instead.
  const uint32_t m_ops_len = dds_stream_countops (desc->m_ops, desc->m_nkeys, desc->m_keys);
  size += m_ops_len * sizeof (uint32_t);

  // m_meta (length + string)
  size += sizeof (uint32_t);
  if (desc->m_meta)
  {
    size += strlen (desc->m_meta) + 1;
  }

  // type_information
  size += sizeof (uint32_t) + desc->type_information.sz;

  // type_mapping
  size += sizeof (uint32_t) + desc->type_mapping.sz;

  return size;
}

// Serialize dds_topic_descriptor to buffer
size_t dds_topic_descriptor_serialize (const dds_topic_descriptor_t *desc, void *buffer, size_t buffer_size)
{
  if (!desc || !buffer)
  {
    return 0;
  }

  size_t required_size = dds_topic_descriptor_serialized_size (desc);
  if (buffer_size < required_size)
  {
    return 0;
  }

  uint8_t *ptr = (uint8_t *)buffer;

  // Serialize fixed fields
  memcpy (ptr, &desc->m_size, sizeof (desc->m_size));
  ptr += sizeof (desc->m_size);

  memcpy (ptr, &desc->m_align, sizeof (desc->m_align));
  ptr += sizeof (desc->m_align);

  memcpy (ptr, &desc->m_flagset, sizeof (desc->m_flagset));
  ptr += sizeof (desc->m_flagset);

  memcpy (ptr, &desc->m_nkeys, sizeof (desc->m_nkeys));
  ptr += sizeof (desc->m_nkeys);

  const uint32_t m_ops_len = dds_stream_countops (desc->m_ops, desc->m_nkeys, desc->m_keys);
  memcpy (ptr, &m_ops_len, sizeof (m_ops_len));
  ptr += sizeof (m_ops_len);

  memcpy (ptr, &desc->restrict_data_representation, sizeof (desc->restrict_data_representation));
  ptr += sizeof (desc->restrict_data_representation);

  // Serialize m_typename
  uint32_t typename_len = desc->m_typename ? (uint32_t)strlen (desc->m_typename) + 1 : 0;
  memcpy (ptr, &typename_len, sizeof (uint32_t));
  ptr += sizeof (uint32_t);
  if (typename_len > 0)
  {
    memcpy (ptr, desc->m_typename, typename_len);
    ptr += typename_len;
  }

  // Serialize m_keys
  for (uint32_t i = 0; i < desc->m_nkeys; i++)
  {
    memcpy (ptr, &desc->m_keys[i].m_offset, sizeof (uint32_t));
    ptr += sizeof (uint32_t);
    memcpy (ptr, &desc->m_keys[i].m_idx, sizeof (uint32_t));
    ptr += sizeof (uint32_t);

    uint32_t name_len = desc->m_keys[i].m_name ? (uint32_t)strlen (desc->m_keys[i].m_name) + 1 : 0;
    memcpy (ptr, &name_len, sizeof (uint32_t));
    ptr += sizeof (uint32_t);
    if (name_len > 0)
    {
      memcpy (ptr, desc->m_keys[i].m_name, name_len);
      ptr += name_len;
    }
  }

  // Serialize m_ops
  if (m_ops_len > 0)
  {
    memcpy (ptr, desc->m_ops, m_ops_len * sizeof (uint32_t));
    ptr += m_ops_len * sizeof (uint32_t);
  }

  // Serialize m_meta
  uint32_t meta_len = desc->m_meta ? (uint32_t)strlen (desc->m_meta) + 1 : 0;
  memcpy (ptr, &meta_len, sizeof (uint32_t));
  ptr += sizeof (uint32_t);
  if (meta_len > 0)
  {
    memcpy (ptr, desc->m_meta, meta_len);
    ptr += meta_len;
  }

  // Serialize type_information
  memcpy (ptr, &desc->type_information.sz, sizeof (uint32_t));
  ptr += sizeof (uint32_t);
  if (desc->type_information.sz > 0)
  {
    memcpy (ptr, desc->type_information.data, desc->type_information.sz);
    ptr += desc->type_information.sz;
  }

  // Serialize type_mapping
  memcpy (ptr, &desc->type_mapping.sz, sizeof (uint32_t));
  ptr += sizeof (uint32_t);
  if (desc->type_mapping.sz > 0)
  {
    memcpy (ptr, desc->type_mapping.data, desc->type_mapping.sz);
    ptr += desc->type_mapping.sz;
  }

  return (size_t)(ptr - (uint8_t *)buffer);
}

// Deserialize dds_topic_descriptor from buffer
dds_topic_descriptor_t *dds_topic_descriptor_deserialize (const void *buffer, size_t buffer_size)
{
  if (!buffer || buffer_size == 0)
  {
    return NULL;
  }

  dds_topic_descriptor_t *desc = (dds_topic_descriptor_t *)calloc (1, sizeof (dds_topic_descriptor_t));
  if (!desc)
  {
    return NULL;
  }

  const uint8_t *ptr = (const uint8_t *)buffer;
  const uint8_t *end = ptr + buffer_size;

  // Deserialize fixed fields
  if (ptr + sizeof (desc->m_size) > end)
    goto error;
  memcpy ((void *)&desc->m_size, ptr, sizeof (desc->m_size));
  ptr += sizeof (desc->m_size);

  if (ptr + sizeof (desc->m_align) > end)
    goto error;
  memcpy ((void *)&desc->m_align, ptr, sizeof (desc->m_align));
  ptr += sizeof (desc->m_align);

  if (ptr + sizeof (desc->m_flagset) > end)
    goto error;
  memcpy ((void *)&desc->m_flagset, ptr, sizeof (desc->m_flagset));
  ptr += sizeof (desc->m_flagset);

  if (ptr + sizeof (desc->m_nkeys) > end)
    goto error;
  memcpy ((void *)&desc->m_nkeys, ptr, sizeof (desc->m_nkeys));
  ptr += sizeof (desc->m_nkeys);

  
  if (ptr + sizeof (desc->m_nops) > end)
    goto error;
  memcpy ((void *)&desc->m_nops, ptr, sizeof (desc->m_nops));
  ptr += sizeof (desc->m_nops);

  if (ptr + sizeof (desc->restrict_data_representation) > end)
    goto error;
  memcpy ((void *)&desc->restrict_data_representation, ptr, sizeof (desc->restrict_data_representation));
  ptr += sizeof (desc->restrict_data_representation);

  // Deserialize m_typename
  uint32_t typename_len;
  if (ptr + sizeof (uint32_t) > end)
    goto error;
  memcpy (&typename_len, ptr, sizeof (uint32_t));
  ptr += sizeof (uint32_t);
  if (typename_len > 0)
  {
    if (ptr + typename_len > end)
      goto error;
    char *typename = (char *)malloc (typename_len);
    if (!typename)
      goto error;
    memcpy (typename, ptr, typename_len);
    *(char **)&desc->m_typename = typename;
    ptr += typename_len;
  }

  // Deserialize m_keys
  if (desc->m_nkeys > 0)
  {
    dds_key_descriptor_t *keys = (dds_key_descriptor_t *)calloc (desc->m_nkeys, sizeof (dds_key_descriptor_t));
    if (!keys)
      goto error;
    *(dds_key_descriptor_t **)&desc->m_keys = keys;

    for (uint32_t i = 0; i < desc->m_nkeys; i++)
    {
      if (ptr + sizeof (uint32_t) * 2 > end)
        goto error;
      memcpy (&keys[i].m_offset, ptr, sizeof (uint32_t));
      ptr += sizeof (uint32_t);
      memcpy (&keys[i].m_idx, ptr, sizeof (uint32_t));
      ptr += sizeof (uint32_t);

      uint32_t name_len;
      if (ptr + sizeof (uint32_t) > end)
        goto error;
      memcpy (&name_len, ptr, sizeof (uint32_t));
      ptr += sizeof (uint32_t);
      if (name_len > 0)
      {
        if (ptr + name_len > end)
          goto error;
        char *name = (char *)malloc (name_len);
        if (!name)
          goto error;
        memcpy (name, ptr, name_len);
        keys[i].m_name = name;
        ptr += name_len;
      }
    }
  }

  // Deserialize m_ops
  if (desc->m_nops > 0)
  {
    if (ptr + desc->m_nops * sizeof (uint32_t) > end)
      goto error;
    uint32_t *ops = (uint32_t *)malloc (desc->m_nops * sizeof (uint32_t));
    if (!ops)
      goto error;
    memcpy (ops, ptr, desc->m_nops * sizeof (uint32_t));
    *(uint32_t **)&desc->m_ops = ops;
    ptr += desc->m_nops * sizeof (uint32_t);
  }

  // Deserialize m_meta
  uint32_t meta_len;
  if (ptr + sizeof (uint32_t) > end)
    goto error;
  memcpy (&meta_len, ptr, sizeof (uint32_t));
  ptr += sizeof (uint32_t);
  if (meta_len > 0)
  {
    if (ptr + meta_len > end)
      goto error;
    char *meta = (char *)malloc (meta_len);
    if (!meta)
      goto error;
    memcpy (meta, ptr, meta_len);
    *(char **)&desc->m_meta = meta;
    ptr += meta_len;
  }

  // Deserialize type_information
  uint32_t type_info_sz;
  if (ptr + sizeof (uint32_t) > end)
    goto error;
  memcpy (&type_info_sz, ptr, sizeof (uint32_t));
  ptr += sizeof (uint32_t);
  *(uint32_t *)&desc->type_information.sz = type_info_sz;
  if (type_info_sz > 0)
  {
    if (ptr + type_info_sz > end)
      goto error;
    unsigned char *type_info_data = (unsigned char *)malloc (type_info_sz);
    if (!type_info_data)
      goto error;
    memcpy (type_info_data, ptr, type_info_sz);
    *(const unsigned char **)&desc->type_information.data = type_info_data;
    ptr += type_info_sz;
  }

  // Deserialize type_mapping
  uint32_t type_map_sz;
  if (ptr + sizeof (uint32_t) > end)
    goto error;
  memcpy (&type_map_sz, ptr, sizeof (uint32_t));
  ptr += sizeof (uint32_t);
  *(uint32_t *)&desc->type_mapping.sz = type_map_sz;
  if (type_map_sz > 0)
  {
    if (ptr + type_map_sz > end)
      goto error;
    unsigned char *type_map_data = (unsigned char *)malloc (type_map_sz);
    if (!type_map_data)
      goto error;
    memcpy (type_map_data, ptr, type_map_sz);
    *(const unsigned char **)&desc->type_mapping.data = type_map_data;
  }
  assert (ptr == end);

  return desc;

error:
  dds_topic_descriptor_free (desc);
  return NULL;
}

// Free deserialized dds_topic_descriptor
void dds_topic_descriptor_free (dds_topic_descriptor_t *desc)
{
  if (!desc)
  {
    return;
  }

  free ((void *)desc->m_typename);

  if (desc->m_keys)
  {
    for (uint32_t i = 0; i < desc->m_nkeys; i++)
    {
      free ((void *)desc->m_keys[i].m_name);
    }
    free ((void *)desc->m_keys);
  }

  free ((void *)desc->m_ops);
  free ((void *)desc->m_meta);
  free ((void *)desc->type_information.data);
  free ((void *)desc->type_mapping.data);

  free (desc);
}
