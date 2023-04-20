// Copyright(c) 2019 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "CUnit/Theory.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/endian.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/ddsi_plist.h"
#include "ddsi__plist.h"
#include "dds/features.h"

CU_Test (ddsi_plist, unalias_copy_merge)
{
  /* one int, one string and one string sequence covers most cases */
  ddsi_plist_t p0, p0memcpy;
  char *p0strs[7];
  ddsi_plist_init_empty (&p0);
  p0.qos.present = DDSI_QP_PARTITION;
  p0.qos.aliased = DDSI_QP_PARTITION;
  p0.qos.partition.n = 3;
  p0.qos.partition.strs = ddsrt_malloc (p0.qos.partition.n * sizeof (*p0.qos.partition.strs));
  p0strs[0] = p0.qos.partition.strs[0] = "aap";
  p0strs[1] = p0.qos.partition.strs[1] = "noot";
  p0strs[2] = p0.qos.partition.strs[2] = "mies";
#ifdef DDS_HAS_SECURITY
  p0.present |= PP_IDENTITY_TOKEN;
  p0.aliased |= PP_IDENTITY_TOKEN;
  p0.identity_token.class_id = "class_id";
  p0.identity_token.properties.n = 2;
  p0.identity_token.properties.props = ddsrt_malloc (p0.identity_token.properties.n * sizeof (*p0.identity_token.properties.props));
  p0.identity_token.properties.props[0].propagate = false;
  p0strs[3] = p0.identity_token.properties.props[0].name = "name0";
  p0strs[4] = p0.identity_token.properties.props[0].value = "value0";
  p0.identity_token.properties.props[1].propagate = true;
  p0strs[5] = p0.identity_token.properties.props[1].name = "name1";
  p0strs[6] = p0.identity_token.properties.props[1].value = "value1";
  p0.identity_token.binary_properties.n = 0;
  p0.identity_token.binary_properties.props = NULL;
#endif
  memcpy (&p0memcpy, &p0, sizeof (p0));

  /* manually alias one, so we can free it*/
  ddsi_plist_t p0alias;
  memcpy (&p0alias, &p0, sizeof (p0));
  p0alias.qos.partition.strs = ddsrt_memdup (p0alias.qos.partition.strs, p0.qos.partition.n * sizeof (*p0.qos.partition.strs));
#ifdef DDS_HAS_SECURITY
  p0alias.identity_token.properties.props = ddsrt_memdup (p0alias.identity_token.properties.props,
                                              p0.identity_token.properties.n * sizeof (*p0.identity_token.properties.props));
#endif
  ddsi_plist_fini (&p0alias);
  CU_ASSERT (memcmp (&p0, &p0memcpy, sizeof (p0)) == 0);
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[0], p0strs[0]);
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[1], p0strs[1]);
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[2], p0strs[2]);
#ifdef DDS_HAS_SECURITY
  CU_ASSERT_STRING_EQUAL (p0.identity_token.properties.props[0].name,  p0strs[3]);
  CU_ASSERT_STRING_EQUAL (p0.identity_token.properties.props[0].value, p0strs[4]);
  CU_ASSERT_STRING_EQUAL (p0.identity_token.properties.props[1].name,  p0strs[5]);
  CU_ASSERT_STRING_EQUAL (p0.identity_token.properties.props[1].value, p0strs[6]);
#endif

  /* copy an aliased one; the original must be unchanged, the copy unaliased */
  ddsi_plist_t p1;
  ddsi_plist_init_empty (&p1);
  ddsi_plist_copy (&p1, &p0);
  CU_ASSERT (memcmp (&p0, &p0memcpy, sizeof (p0)) == 0);
  CU_ASSERT (p1.present == p0.present);
  CU_ASSERT (p1.aliased == 0);
  CU_ASSERT (p1.qos.present == p0.qos.present);
  CU_ASSERT (p1.qos.aliased == 0);
  CU_ASSERT (p1.qos.partition.n == p0.qos.partition.n);
  CU_ASSERT (p1.qos.partition.strs != p0.qos.partition.strs);
  CU_ASSERT (p1.qos.partition.strs[0] != p0.qos.partition.strs[0]);
  CU_ASSERT (p1.qos.partition.strs[1] != p0.qos.partition.strs[1]);
  CU_ASSERT (p1.qos.partition.strs[2] != p0.qos.partition.strs[2]);
  CU_ASSERT_STRING_EQUAL (p1.qos.partition.strs[0], p0.qos.partition.strs[0]);
  CU_ASSERT_STRING_EQUAL (p1.qos.partition.strs[1], p0.qos.partition.strs[1]);
  CU_ASSERT_STRING_EQUAL (p1.qos.partition.strs[2], p0.qos.partition.strs[2]);
#ifdef DDS_HAS_SECURITY
  CU_ASSERT (p1.identity_token.class_id != p0.identity_token.class_id);
  CU_ASSERT_STRING_EQUAL (p1.identity_token.class_id, p0.identity_token.class_id);
  CU_ASSERT (p1.identity_token.properties.n == p0.identity_token.properties.n);
  CU_ASSERT (p1.identity_token.properties.props != p0.identity_token.properties.props);
  CU_ASSERT (p1.identity_token.properties.props[0].name != p0.identity_token.properties.props[0].name);
  CU_ASSERT (p1.identity_token.properties.props[0].value != p0.identity_token.properties.props[0].value);
  CU_ASSERT (p1.identity_token.properties.props[0].propagate == p0.identity_token.properties.props[0].propagate);
  CU_ASSERT (p1.identity_token.properties.props[1].name != p0.identity_token.properties.props[1].name);
  CU_ASSERT (p1.identity_token.properties.props[1].value != p0.identity_token.properties.props[1].value);
  CU_ASSERT (p1.identity_token.properties.props[1].propagate == p0.identity_token.properties.props[1].propagate);
  CU_ASSERT_STRING_EQUAL (p1.identity_token.properties.props[0].name, p0.identity_token.properties.props[0].name);
  CU_ASSERT_STRING_EQUAL (p1.identity_token.properties.props[0].value, p0.identity_token.properties.props[0].value);
  CU_ASSERT_STRING_EQUAL (p1.identity_token.properties.props[1].name, p0.identity_token.properties.props[1].name);
  CU_ASSERT_STRING_EQUAL (p1.identity_token.properties.props[1].value, p0.identity_token.properties.props[1].value);
  CU_ASSERT (p1.identity_token.binary_properties.n == 0);
  CU_ASSERT (p1.identity_token.binary_properties.props == NULL);
#endif

  /* merge-in missing ones from an aliased copy: original must remain unchanged;
     existing ones should stay without touching "aliased" only new ones are
     added as unaliased ones */
  ddsi_plist_t p2, p2memcpy;
  ddsi_plist_init_empty (&p2);
  memcpy (&p2memcpy, &p2, sizeof (p2));
  ddsi_plist_mergein_missing (&p2, &p0, p0.present, p0.qos.present);
  CU_ASSERT (memcmp (&p0, &p0memcpy, sizeof (p0)) == 0);
  CU_ASSERT (p2.present == p0.present);
  CU_ASSERT (p2.aliased == p2memcpy.aliased);
  CU_ASSERT (p2.qos.present == p0.qos.present);
  CU_ASSERT (p2.qos.aliased == p2memcpy.qos.aliased);
  CU_ASSERT (p2.qos.partition.n == p0.qos.partition.n);
  CU_ASSERT (p2.qos.partition.strs != p0.qos.partition.strs);
  CU_ASSERT (p2.qos.partition.strs[0] != p0.qos.partition.strs[0]);
  CU_ASSERT (p2.qos.partition.strs[1] != p0.qos.partition.strs[1]);
  CU_ASSERT (p2.qos.partition.strs[2] != p0.qos.partition.strs[2]);
  CU_ASSERT_STRING_EQUAL (p2.qos.partition.strs[0], p0.qos.partition.strs[0]);
  CU_ASSERT_STRING_EQUAL (p2.qos.partition.strs[1], p0.qos.partition.strs[1]);
  CU_ASSERT_STRING_EQUAL (p2.qos.partition.strs[2], p0.qos.partition.strs[2]);
#ifdef DDS_HAS_SECURITY
  CU_ASSERT (p2.identity_token.class_id != p0.identity_token.class_id);
  CU_ASSERT_STRING_EQUAL (p2.identity_token.class_id, p0.identity_token.class_id);
  CU_ASSERT (p2.identity_token.properties.n == p0.identity_token.properties.n);
  CU_ASSERT (p2.identity_token.properties.props != p0.identity_token.properties.props);
  CU_ASSERT (p2.identity_token.properties.props[0].name != p0.identity_token.properties.props[0].name);
  CU_ASSERT (p2.identity_token.properties.props[0].value != p0.identity_token.properties.props[0].value);
  CU_ASSERT (p2.identity_token.properties.props[0].propagate == p0.identity_token.properties.props[0].propagate);
  CU_ASSERT (p2.identity_token.properties.props[1].name != p0.identity_token.properties.props[1].name);
  CU_ASSERT (p2.identity_token.properties.props[1].value != p0.identity_token.properties.props[1].value);
  CU_ASSERT (p2.identity_token.properties.props[1].propagate == p0.identity_token.properties.props[1].propagate);
  CU_ASSERT_STRING_EQUAL (p2.identity_token.properties.props[0].name, p0.identity_token.properties.props[0].name);
  CU_ASSERT_STRING_EQUAL (p2.identity_token.properties.props[0].value, p0.identity_token.properties.props[0].value);
  CU_ASSERT_STRING_EQUAL (p2.identity_token.properties.props[1].name, p0.identity_token.properties.props[1].name);
  CU_ASSERT_STRING_EQUAL (p2.identity_token.properties.props[1].value, p0.identity_token.properties.props[1].value);
  CU_ASSERT (p2.identity_token.binary_properties.n == 0);
  CU_ASSERT (p2.identity_token.binary_properties.props == NULL);
#endif

  /* unalias of p0, partition.strs mustn't change, because it, unlike its elements, wasn't aliased */
  ddsi_plist_unalias (&p0);
  CU_ASSERT (p0.present == p0memcpy.present);
  CU_ASSERT (p0.aliased == 0);
  CU_ASSERT (p0.qos.present == p0memcpy.qos.present);
  CU_ASSERT (p0.qos.aliased == 0);
  CU_ASSERT (p0.qos.partition.n == p0memcpy.qos.partition.n);
  CU_ASSERT (p0.qos.partition.strs == p0memcpy.qos.partition.strs);
  CU_ASSERT (p0.qos.partition.strs[0] != p0strs[0]);
  CU_ASSERT (p0.qos.partition.strs[1] != p0strs[1]);
  CU_ASSERT (p0.qos.partition.strs[2] != p0strs[2]);
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[0], p0strs[0]);
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[1], p0strs[1]);
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[2], p0strs[2]);
#ifdef DDS_HAS_SECURITY
  CU_ASSERT (p0.identity_token.properties.props[0].name  != p0strs[3]);
  CU_ASSERT (p0.identity_token.properties.props[0].value != p0strs[4]);
  CU_ASSERT (p0.identity_token.properties.props[1].name  != p0strs[5]);
  CU_ASSERT (p0.identity_token.properties.props[1].value != p0strs[6]);
  CU_ASSERT_STRING_EQUAL (p0.identity_token.properties.props[0].name,  p0strs[3]);
  CU_ASSERT_STRING_EQUAL (p0.identity_token.properties.props[0].value, p0strs[4]);
  CU_ASSERT_STRING_EQUAL (p0.identity_token.properties.props[1].name,  p0strs[5]);
  CU_ASSERT_STRING_EQUAL (p0.identity_token.properties.props[1].value, p0strs[6]);
#endif

  memcpy (&p0memcpy, &p0, sizeof (p0));

  /* copy an aliased one; the original must be unchanged, the copy unaliased */
  ddsi_plist_t p3;
  ddsi_plist_init_empty (&p3);
  ddsi_plist_copy (&p3, &p0);
  CU_ASSERT (memcmp (&p0, &p0memcpy, sizeof (p0)) == 0);
  CU_ASSERT (p3.present == p0.present);
  CU_ASSERT (p3.aliased == 0);
  CU_ASSERT (p3.qos.present == p0.qos.present);
  CU_ASSERT (p3.qos.aliased == 0);
  CU_ASSERT (p3.qos.partition.n == p0.qos.partition.n);
  CU_ASSERT (p3.qos.partition.strs != p0.qos.partition.strs);
  CU_ASSERT (p3.qos.partition.strs[0] != p0.qos.partition.strs[0]);
  CU_ASSERT (p3.qos.partition.strs[1] != p0.qos.partition.strs[1]);
  CU_ASSERT (p3.qos.partition.strs[2] != p0.qos.partition.strs[2]);
  CU_ASSERT_STRING_EQUAL (p3.qos.partition.strs[0], p0.qos.partition.strs[0]);
  CU_ASSERT_STRING_EQUAL (p3.qos.partition.strs[1], p0.qos.partition.strs[1]);
  CU_ASSERT_STRING_EQUAL (p3.qos.partition.strs[2], p0.qos.partition.strs[2]);
#ifdef DDS_HAS_SECURITY
  CU_ASSERT (p3.identity_token.class_id != p0.identity_token.class_id);
  CU_ASSERT_STRING_EQUAL (p3.identity_token.class_id, p0.identity_token.class_id);
  CU_ASSERT (p3.identity_token.properties.n == p0.identity_token.properties.n);
  CU_ASSERT (p3.identity_token.properties.props != p0.identity_token.properties.props);
  CU_ASSERT (p3.identity_token.properties.props[0].name != p0.identity_token.properties.props[0].name);
  CU_ASSERT (p3.identity_token.properties.props[0].value != p0.identity_token.properties.props[0].value);
  CU_ASSERT (p3.identity_token.properties.props[0].propagate == p0.identity_token.properties.props[0].propagate);
  CU_ASSERT (p3.identity_token.properties.props[1].name != p0.identity_token.properties.props[1].name);
  CU_ASSERT (p3.identity_token.properties.props[1].value != p0.identity_token.properties.props[1].value);
  CU_ASSERT (p3.identity_token.properties.props[1].propagate == p0.identity_token.properties.props[1].propagate);
  CU_ASSERT_STRING_EQUAL (p3.identity_token.properties.props[0].name, p0.identity_token.properties.props[0].name);
  CU_ASSERT_STRING_EQUAL (p3.identity_token.properties.props[0].value, p0.identity_token.properties.props[0].value);
  CU_ASSERT_STRING_EQUAL (p3.identity_token.properties.props[1].name, p0.identity_token.properties.props[1].name);
  CU_ASSERT_STRING_EQUAL (p3.identity_token.properties.props[1].value, p0.identity_token.properties.props[1].value);
  CU_ASSERT (p3.identity_token.binary_properties.n == 0);
  CU_ASSERT (p3.identity_token.binary_properties.props == NULL);
#endif

  /* merge-in missing ones from an aliased copy: original must remain unchanged;
     existing ones should stay without touching "aliased" only new ones are
     added as unaliased ones */
  ddsi_plist_t p4, p4memcpy;
  ddsi_plist_init_empty (&p4);
  memcpy (&p4memcpy, &p4, sizeof (p4));
  ddsi_plist_mergein_missing (&p4, &p0, p0.present, p0.qos.present);
  CU_ASSERT (memcmp (&p0, &p0memcpy, sizeof (p0)) == 0);
  CU_ASSERT (p4.present == p0.present);
  CU_ASSERT (p4.aliased == p4memcpy.aliased);
  CU_ASSERT (p4.qos.present == p0.qos.present);
  CU_ASSERT (p4.qos.aliased == p4memcpy.qos.aliased);
  CU_ASSERT (p4.qos.partition.n == p0.qos.partition.n);
  CU_ASSERT (p4.qos.partition.strs != p0.qos.partition.strs);
  CU_ASSERT (p4.qos.partition.strs[0] != p0.qos.partition.strs[0]);
  CU_ASSERT (p4.qos.partition.strs[1] != p0.qos.partition.strs[1]);
  CU_ASSERT (p4.qos.partition.strs[2] != p0.qos.partition.strs[2]);
  CU_ASSERT_STRING_EQUAL (p4.qos.partition.strs[0], p0.qos.partition.strs[0]);
  CU_ASSERT_STRING_EQUAL (p4.qos.partition.strs[1], p0.qos.partition.strs[1]);
  CU_ASSERT_STRING_EQUAL (p4.qos.partition.strs[2], p0.qos.partition.strs[2]);
#ifdef DDS_HAS_SECURITY
  CU_ASSERT (p4.identity_token.class_id != p0.identity_token.class_id);
  CU_ASSERT_STRING_EQUAL (p4.identity_token.class_id, p0.identity_token.class_id);
  CU_ASSERT (p4.identity_token.properties.n == p0.identity_token.properties.n);
  CU_ASSERT (p4.identity_token.properties.props != p0.identity_token.properties.props);
  CU_ASSERT (p4.identity_token.properties.props[0].name != p0.identity_token.properties.props[0].name);
  CU_ASSERT (p4.identity_token.properties.props[0].value != p0.identity_token.properties.props[0].value);
  CU_ASSERT (p4.identity_token.properties.props[0].propagate == p0.identity_token.properties.props[0].propagate);
  CU_ASSERT (p4.identity_token.properties.props[1].name != p0.identity_token.properties.props[1].name);
  CU_ASSERT (p4.identity_token.properties.props[1].value != p0.identity_token.properties.props[1].value);
  CU_ASSERT (p4.identity_token.properties.props[1].propagate == p0.identity_token.properties.props[1].propagate);
  CU_ASSERT_STRING_EQUAL (p4.identity_token.properties.props[0].name, p0.identity_token.properties.props[0].name);
  CU_ASSERT_STRING_EQUAL (p4.identity_token.properties.props[0].value, p0.identity_token.properties.props[0].value);
  CU_ASSERT_STRING_EQUAL (p4.identity_token.properties.props[1].name, p0.identity_token.properties.props[1].name);
  CU_ASSERT_STRING_EQUAL (p4.identity_token.properties.props[1].value, p0.identity_token.properties.props[1].value);
  CU_ASSERT (p4.identity_token.binary_properties.n == 0);
  CU_ASSERT (p4.identity_token.binary_properties.props == NULL);
#endif

  ddsi_plist_fini (&p0);
  ddsi_plist_fini (&p1);
  ddsi_plist_fini (&p2);
  ddsi_plist_fini (&p3);
  ddsi_plist_fini (&p4);
}
