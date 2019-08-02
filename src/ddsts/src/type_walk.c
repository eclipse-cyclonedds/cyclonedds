/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "dds/ddsts/typetree.h"
#include "dds/ddsts/type_walk.h"

dds_return_t ddsts_walk(ddsts_call_path_t *path, ddsts_flags_t visit, ddsts_flags_t call, ddsts_walk_call_func_t func, void *context)
{
  ddsts_call_path_t child_path;
  child_path.call_parent = path;
  ddsts_type_t *child = NULL;
  if (DDSTS_IS_TYPE(path->type, DDSTS_MODULE)) {
    child = path->type->module.members.first;
  }
  else if (DDSTS_IS_TYPE(path->type, DDSTS_STRUCT)) {
    child = path->type->struct_def.members.first;
  }
  for (; child != NULL; child = child->type.next) {
    child_path.type = child;
    if ((DDSTS_TYPE_OF(child) & call) != 0) {
      dds_return_t rc = func(&child_path, context);
      if (rc != DDS_RETCODE_OK) {
        return rc;
      }
    }
    if ((DDSTS_TYPE_OF(child) & visit) != 0) {
      dds_return_t rc = ddsts_walk(&child_path, visit, call, func, context);
      if (rc != DDS_RETCODE_OK) {
        return rc;
      }
    }
  }
  return DDS_RETCODE_OK;
}

