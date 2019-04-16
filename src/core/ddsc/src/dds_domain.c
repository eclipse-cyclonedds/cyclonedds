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
#include "dds__domain.h"
#include "dds/ddsi/ddsi_tkmap.h"

static int dds_domain_compare (const int32_t * a, const int32_t * b)
{
  return (*a == *b) ? 0 : (*a < *b) ? -1 : 1;
}

const ddsrt_avl_treedef_t dds_domaintree_def = DDSRT_AVL_TREEDEF_INITIALIZER
(
  offsetof (dds_domain, m_node),
  offsetof (dds_domain, m_id),
  (int (*) (const void *, const void *)) dds_domain_compare,
  0
);

dds_domain * dds_domain_find_locked (dds_domainid_t id)
{
  return ddsrt_avl_lookup (&dds_domaintree_def, &dds_global.m_domains, &id);
}

dds_domain * dds_domain_create (dds_domainid_t id)
{
  dds_domain * domain;
  ddsrt_mutex_lock (&dds_global.m_mutex);
  domain = dds_domain_find_locked (id);
  if (domain == NULL)
  {
    domain = dds_alloc (sizeof (*domain));
    domain->m_id = id;
    ddsrt_avl_init (&dds_topictree_def, &domain->m_topics);
    ddsrt_avl_insert (&dds_domaintree_def, &dds_global.m_domains, domain);
  }
  domain->m_refc++;
  ddsrt_mutex_unlock (&dds_global.m_mutex);
  return domain;
}

void dds_domain_free (dds_domain * domain)
{
  ddsrt_mutex_lock (&dds_global.m_mutex);
  if (--domain->m_refc == 0)
  {
    ddsrt_avl_delete (&dds_domaintree_def, &dds_global.m_domains, domain);
    dds_free (domain);
  }
  ddsrt_mutex_unlock (&dds_global.m_mutex);
}
