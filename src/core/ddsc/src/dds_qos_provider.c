
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

#include "dds/ddsrt/io.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/hopscotch.h"

#include "dds__sysdef_model.h"
#include "dds__sysdef_parser.h"
#include "dds__sysdef_validation.h"
#include "dds__qos_provider.h"
#include "dds__qos.h"

static dds_return_t read_sysdef (const char *path, struct dds_sysdef_system **sysdef)
{
  dds_return_t ret = DDS_RETCODE_BAD_PARAMETER;
  if (path == NULL)
    return ret;
  if (path[0] == '<')
  {
    ret = dds_sysdef_init_sysdef_str(path, sysdef, SYSDEF_SCOPE_QOS_LIB);
  } else {
    FILE *fp;
  DDSRT_WARNING_MSVC_OFF(4996)
    if ((fp = fopen (path, "r")) == NULL)
    {
      SYSDEF_ERROR ("Error reading system definition: can't read from path '%s'\n", path);
      ret = DDS_RETCODE_BAD_PARAMETER;
    } else {
      ret = dds_sysdef_init_sysdef (fp, sysdef, SYSDEF_SCOPE_QOS_LIB);
      (void)fclose(fp);
  DDSRT_WARNING_MSVC_ON(4996)
    }
  }

  return ret;
}

static uint32_t qos_item_hash_fn(const void *a)
{
  dds_qos_item_t *item = (dds_qos_item_t *)a;
  uint32_t x = ddsrt_mh3(&item->kind, sizeof(item->kind), 0);
  x = ddsrt_mh3(item->full_name, strlen(item->full_name), x);
  return x;
}

static bool qos_item_equals_fn(const void *a, const void *b)
{
  dds_qos_item_t *aa = (dds_qos_item_t *)a;
  dds_qos_item_t *bb = (dds_qos_item_t *)b;

  return aa->kind == bb->kind && strcmp(aa->full_name, bb->full_name) == 0;
}

static void cleanup_qos_items (void *vnode, void *varg)
{
  (void) varg;
  dds_qos_item_t *d = (dds_qos_item_t *) vnode;
  ddsrt_free (d->full_name);
  dds_delete_qos(d->qos);
  ddsrt_free(d);
}

#define PROVIDER_ALLOWED_QOS_MASK \
  (DDS_TOPIC_QOS_MASK | DDS_READER_QOS_MASK | DDS_WRITER_QOS_MASK | \
   DDS_SUBSCRIBER_QOS_MASK | DDS_PUBLISHER_QOS_MASK | DDS_PARTICIPANT_QOS_MASK) ^ \
  (DDSI_QP_ENTITY_NAME | DDSI_QP_ADLINK_ENTITY_FACTORY | \
    DDSI_QP_CYCLONE_IGNORELOCAL | DDSI_QP_PSMX)
static dds_return_t read_validate_sysdef(const char *path, struct dds_sysdef_system **sysdef)
{
  dds_return_t ret = DDS_RETCODE_OK;
  struct dds_sysdef_system *def;
  if ((ret = read_sysdef (path, &def)) != DDS_RETCODE_OK)
  {
    QOSPROV_ERROR("Failed during read sysdef: %s\n", path);
    goto err_read;
  }
  if ((ret = dds_validate_qos_lib (def, PROVIDER_ALLOWED_QOS_MASK)) != DDS_RETCODE_OK)
  {
    QOSPROV_ERROR("Failed during validate sysdef: %s\n", path);
    goto err_validate;
  }
  *sysdef = def;

  return ret;
err_validate:
  dds_sysdef_fini_sysdef(def);
err_read:
  return ret;
}
#undef PROVIDER_ALLOWED_QOS_MASK

static dds_return_t init_qos_provider (const struct dds_sysdef_system *sysdef, const char *path, dds_qos_provider_t **provider, char *lib_scope, char *prof_scope, char *ent_scope)
{
  dds_return_t ret = DDS_RETCODE_OK;
  dds_qos_provider_t *qos_provider = (dds_qos_provider_t *)ddsrt_malloc(sizeof(*qos_provider));
  struct ddsrt_hh *keyed_qos = ddsrt_hh_new(1, qos_item_hash_fn, qos_item_equals_fn);
  for (const struct dds_sysdef_qos_lib *lib = sysdef->qos_libs; lib != NULL; lib = (const struct dds_sysdef_qos_lib *)lib->xmlnode.next)
  {
    char *lib_name = lib->name;
    if (lib_scope != NULL && strcmp(lib_scope, PROVIDER_ITEM_SCOPE_NONE) != 0 && strcmp(lib_name, lib_scope) != 0)
      continue;
    for (const struct dds_sysdef_qos_profile *prof = lib->qos_profiles; prof != NULL; prof = (const struct dds_sysdef_qos_profile *)prof->xmlnode.next)
    {
      char *prof_name = prof->name;
      if (prof_scope != NULL && strcmp(prof_scope, PROVIDER_ITEM_SCOPE_NONE) != 0 && strcmp(prof_name, prof_scope) != 0)
        continue;
      char *prefix;
      (void) ddsrt_asprintf(&prefix, "%s"PROVIDER_ITEM_SEP"%s", lib_name, prof_name);
      for (const struct dds_sysdef_qos *qos = prof->qos; qos != NULL; qos = (const struct dds_sysdef_qos *)qos->xmlnode.next)
      {
        if (ent_scope != NULL && strcmp(ent_scope, PROVIDER_ITEM_SCOPE_NONE) != 0 && (qos->name == NULL || strcmp(qos->name, ent_scope) != 0))
          continue;
        dds_qos_item_t *item = ddsrt_malloc(sizeof(*item));
        dds_qos_kind_t kind;
        switch(qos->kind)
        {
          case DDS_SYSDEF_TOPIC_QOS:
            kind = DDS_TOPIC_QOS;
            break;
          case DDS_SYSDEF_READER_QOS:
            kind = DDS_READER_QOS;
            break;
          case DDS_SYSDEF_WRITER_QOS:
            kind = DDS_WRITER_QOS;
            break;
          case DDS_SYSDEF_SUBSCRIBER_QOS:
            kind = DDS_SUBSCRIBER_QOS;
            break;
          case DDS_SYSDEF_PUBLISHER_QOS:
            kind = DDS_PUBLISHER_QOS;
            break;
          case DDS_SYSDEF_PARTICIPANT_QOS:
            kind = DDS_PARTICIPANT_QOS;
            break;
          default:
            ddsrt_free(prefix);
            ddsrt_free(item);
            goto err_prov;
        }
        item->kind = kind;
        item->qos = dds_create_qos();
        dds_merge_qos(item->qos, qos->qos);
        if (qos->name != NULL)
          (void) ddsrt_asprintf(&item->full_name, "%s"PROVIDER_ITEM_SEP"%s", prefix, qos->name);
        else
          item->full_name = ddsrt_strdup(prefix);
        if (!ddsrt_hh_add(keyed_qos, item))
        {
          QOSPROV_ERROR("Qos duplicate name: %s kind: %d file: %s.\n",
                        item->full_name, item->kind, path);
          ret = DDS_RETCODE_BAD_PARAMETER;
          ddsrt_free(prefix);
          cleanup_qos_items(item, NULL);
          goto err_prov;
        }
      }
      ddsrt_free(prefix);
    }
  }
  qos_provider->file_path = ddsrt_strdup(path);
  qos_provider->keyed_qos = keyed_qos;
  *provider = qos_provider;

  return ret;
err_prov:
  ddsrt_hh_enum(keyed_qos, cleanup_qos_items, NULL);
  ddsrt_hh_free(keyed_qos);
  ddsrt_free(qos_provider);
  return ret;
}

dds_return_t dds_create_qos_provider (const char *path, dds_qos_provider_t **provider)
{
  dds_return_t ret = DDS_RETCODE_OK;
  if ((ret = dds_create_qos_provider_scope (path, provider, PROVIDER_ITEM_SCOPE_NONE)) != DDS_RETCODE_OK)
    goto err;

err:
  return ret;
}

dds_return_t dds_qos_provider_get_qos (const dds_qos_provider_t *provider, dds_qos_kind_t type, const char *key, const dds_qos_t **qos)
{
  dds_return_t ret = DDS_RETCODE_OK;
  if (provider == NULL || provider->keyed_qos == NULL)
  {
    QOSPROV_WARN("Failed to access provider qos\n");
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto err;
  }
  QOSPROV_TRACE("request qos for entity type: %d, scope: %s", type, key);
  dds_qos_item_t it = {.full_name = ddsrt_strdup(key), .kind = type};
  dds_qos_item_t *item;
  if ((item = ddsrt_hh_lookup(provider->keyed_qos, &it)) == NULL)
  {
    QOSPROV_WARN("Failed to get qos with name: %s, kind: %d ref file: %s\n",
                            it.full_name, it.kind, provider->file_path);
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto err2;
  }
  *qos = item->qos;

err2:
  ddsrt_free(it.full_name);
err:
  return ret;
}

static void fill_token(char **start, char **end, char **token)
{
  char *bg = *start;
  char *ed = *end;
  if ((bg != NULL) && (ed = strstr(bg, PROVIDER_ITEM_SEP)) != NULL)
    *token = ((ed-bg) > 0)? ddsrt_strndup(bg, (size_t)(ed-bg)): ddsrt_strdup(PROVIDER_ITEM_SCOPE_NONE);
  else
    *token = (bg != NULL && *bg != '\0')? ddsrt_strdup(bg): ddsrt_strdup(PROVIDER_ITEM_SCOPE_NONE);
  *start = (ed != NULL)? ed + strlen(PROVIDER_ITEM_SEP): NULL;
  *end = ed;
}

static void fill_tokens_str(const char *start, char **lib, char **pro, char **ent)
{
  char *current = ddsrt_strdup(start);
  char *next = current;
  char *ending = next;
  fill_token(&next, &ending, lib);
  fill_token(&next, &ending, pro);
  fill_token(&next, &ending, ent);
  ddsrt_free(current);
}

static void empty_tokens_str(char *lib, char *pro, char *ent)
{
  ddsrt_free(lib);
  ddsrt_free(pro);
  ddsrt_free(ent);
}

static dds_return_t resolve_token(const char *key, char **lib, char **prof, char **ent)
{
  dds_return_t ret = DDS_RETCODE_OK;
  if (key == NULL)
  {
    ret = DDS_RETCODE_ERROR;
    goto err;
  }
  fill_tokens_str(key, lib, prof, ent);
err:
  return ret;
}

dds_return_t dds_create_qos_provider_scope (const char *path, dds_qos_provider_t **provider, const char *key)
{
  dds_return_t ret = DDS_RETCODE_OK;
  struct dds_sysdef_system *sysdef;
  if ((ret = read_validate_sysdef(path, &sysdef)) != DDS_RETCODE_OK)
    return ret;
  char *lib_name = NULL, *prof_name = NULL, *ent_name = NULL;
  (void)resolve_token(key, &lib_name, &prof_name, &ent_name);
  if ((ret = init_qos_provider(sysdef, path, provider, lib_name, prof_name, ent_name)) != DDS_RETCODE_OK)
  {
    QOSPROV_ERROR("Failed to create qos provider file: %s, scope: %s", path, key);
    goto err;
  }
err:
  empty_tokens_str(lib_name, prof_name, ent_name);
  dds_sysdef_fini_sysdef(sysdef);
  return ret;
}

void dds_delete_qos_provider (dds_qos_provider_t *provider)
{
  if (provider)
  {
    ddsrt_hh_enum(provider->keyed_qos, cleanup_qos_items, NULL);
    ddsrt_hh_free(provider->keyed_qos);
    ddsrt_free(provider->file_path);
    ddsrt_free(provider);
  }
}
