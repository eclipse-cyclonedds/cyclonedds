// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "descriptor.h"
#include "test_common.h"

#include "CUnit/Theory.h"

static idl_node_t * get_topic_node(idl_pstate_t *pstate, void *list)
{
  for ( ; list; list = idl_next(list)) {
    if (idl_mask(list) == IDL_MODULE) {
      idl_module_t *node = list;
      idl_node_t *topic = get_topic_node(pstate, node->definitions);
      if (topic)
        return topic;
    } else if (idl_mask(list) == IDL_STRUCT || idl_mask(list) == IDL_UNION) {
      if (idl_is_topic (list, (pstate->config.flags & IDL_FLAG_KEYLIST)))
        return list;
    }
  }
  return NULL;
}

idl_retcode_t generate_test_descriptor (idl_pstate_t *pstate, const char *idl, struct descriptor *descriptor)
{
  idl_retcode_t ret = idl_parse_string(pstate, idl);
  if (ret != IDL_RETCODE_OK)
    return ret;

  idl_node_t *topic = get_topic_node (pstate, pstate->root);
  CU_ASSERT_FATAL (topic != NULL);

  if ((ret = generate_descriptor_impl(pstate, topic, descriptor)) != IDL_RETCODE_OK)
    return ret;
  CU_ASSERT_PTR_NOT_NULL_FATAL (descriptor);
  assert (descriptor); /* static analyzer */

  return ret;
}
