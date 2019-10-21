/*
 * Copyright(c) 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include "CUnit/Theory.h"
#include "cyclonedds/ddsrt/heap.h"
#include "cyclonedds/ddsrt/string.h"
#include "cyclonedds/ddsrt/endian.h"
#include "cyclonedds/ddsi/q_xqos.h"
#include "cyclonedds/ddsi/q_plist.h"

CU_Test (ddsi_plist, unalias_copy_merge)
{
  /* one int, one string and one string sequence covers most cases */
  nn_plist_t p0, p0memcpy;
  char *p0strs[3];
  nn_plist_init_empty (&p0);
  p0.present = PP_PRISMTECH_PROCESS_ID | PP_ENTITY_NAME;
  p0.aliased = PP_ENTITY_NAME;
  p0.process_id = 0x12345678;
  p0.entity_name = "nemo";
  p0.qos.present = QP_PARTITION;
  p0.qos.aliased = QP_PARTITION;
  p0.qos.partition.n = 3;
  p0.qos.partition.strs = ddsrt_malloc (p0.qos.partition.n * sizeof (*p0.qos.partition.strs));
  p0strs[0] = p0.qos.partition.strs[0] = "aap";
  p0strs[1] = p0.qos.partition.strs[1] = "noot";
  p0strs[2] = p0.qos.partition.strs[2] = "mies";
  memcpy (&p0memcpy, &p0, sizeof (p0));

  /* manually alias one, so we can free it*/
  nn_plist_t p0alias;
  memcpy (&p0alias, &p0, sizeof (p0));
  p0alias.qos.partition.strs = ddsrt_memdup (p0alias.qos.partition.strs, p0.qos.partition.n * sizeof (*p0.qos.partition.strs));
  nn_plist_fini (&p0alias);
  CU_ASSERT (memcmp (&p0, &p0memcpy, sizeof (p0)) == 0);
  CU_ASSERT_STRING_EQUAL (p0.entity_name, "nemo");
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[0], p0strs[0]);
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[1], p0strs[1]);
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[2], p0strs[2]);

  /* copy an aliased one; the original must be unchanged, the copy unaliased */
  nn_plist_t p1;
  nn_plist_init_empty (&p1);
  nn_plist_copy (&p1, &p0);
  CU_ASSERT (memcmp (&p0, &p0memcpy, sizeof (p0)) == 0);
  CU_ASSERT (p1.present == p0.present);
  CU_ASSERT (p1.aliased == 0);
  CU_ASSERT (p1.qos.present == p0.qos.present);
  CU_ASSERT (p1.qos.aliased == 0);
  CU_ASSERT (p1.process_id == p0.process_id);
  CU_ASSERT (p1.entity_name != p0.entity_name);
  CU_ASSERT_STRING_EQUAL (p1.entity_name, p0.entity_name);
  CU_ASSERT (p1.qos.partition.n == p0.qos.partition.n);
  CU_ASSERT (p1.qos.partition.strs != p0.qos.partition.strs);
  CU_ASSERT (p1.qos.partition.strs[0] != p0.qos.partition.strs[0]);
  CU_ASSERT (p1.qos.partition.strs[1] != p0.qos.partition.strs[1]);
  CU_ASSERT (p1.qos.partition.strs[2] != p0.qos.partition.strs[2]);
  CU_ASSERT_STRING_EQUAL (p1.qos.partition.strs[0], p0.qos.partition.strs[0]);
  CU_ASSERT_STRING_EQUAL (p1.qos.partition.strs[1], p0.qos.partition.strs[1]);
  CU_ASSERT_STRING_EQUAL (p1.qos.partition.strs[2], p0.qos.partition.strs[2]);

  /* merge-in missing ones from an aliased copy: original must remain unchanged;
     existing ones should stay without touching "aliased" only new ones are
     added as unaliased ones */
  nn_plist_t p2, p2memcpy;
  nn_plist_init_empty (&p2);
  p2.present = PP_ENTITY_NAME;
  p2.aliased = PP_ENTITY_NAME;
  p2.entity_name = "omen";
  memcpy (&p2memcpy, &p2, sizeof (p2));
  nn_plist_mergein_missing (&p2, &p0, p0.present, p0.qos.present);
  CU_ASSERT (memcmp (&p0, &p0memcpy, sizeof (p0)) == 0);
  CU_ASSERT (p2.present == p0.present);
  CU_ASSERT (p2.aliased == p2memcpy.aliased);
  CU_ASSERT (p2.qos.present == p0.qos.present);
  CU_ASSERT (p2.qos.aliased == p2memcpy.qos.aliased);
  CU_ASSERT (p2.process_id == p0.process_id);
  CU_ASSERT (p2.entity_name == p2memcpy.entity_name);
  CU_ASSERT_STRING_EQUAL (p2.entity_name, "omen");
  CU_ASSERT (p2.qos.partition.n == p0.qos.partition.n);
  CU_ASSERT (p2.qos.partition.strs != p0.qos.partition.strs);
  CU_ASSERT (p2.qos.partition.strs[0] != p0.qos.partition.strs[0]);
  CU_ASSERT (p2.qos.partition.strs[1] != p0.qos.partition.strs[1]);
  CU_ASSERT (p2.qos.partition.strs[2] != p0.qos.partition.strs[2]);
  CU_ASSERT_STRING_EQUAL (p2.qos.partition.strs[0], p0.qos.partition.strs[0]);
  CU_ASSERT_STRING_EQUAL (p2.qos.partition.strs[1], p0.qos.partition.strs[1]);
  CU_ASSERT_STRING_EQUAL (p2.qos.partition.strs[2], p0.qos.partition.strs[2]);

  /* unalias of p0, partition.strs mustn't change, because it, unlike its elements, wasn't aliased */
  nn_plist_unalias (&p0);
  CU_ASSERT (p0.present == p0memcpy.present);
  CU_ASSERT (p0.aliased == 0);
  CU_ASSERT (p0.qos.present == p0memcpy.qos.present);
  CU_ASSERT (p0.qos.aliased == 0);
  CU_ASSERT (p0.process_id == p0memcpy.process_id);
  CU_ASSERT (p0.entity_name != p0memcpy.entity_name);
  CU_ASSERT_STRING_EQUAL (p0.entity_name, p0memcpy.entity_name);
  CU_ASSERT (p0.qos.partition.n == p0memcpy.qos.partition.n);
  CU_ASSERT (p0.qos.partition.strs == p0memcpy.qos.partition.strs);
  CU_ASSERT (p0.qos.partition.strs[0] != p0strs[0]);
  CU_ASSERT (p0.qos.partition.strs[1] != p0strs[1]);
  CU_ASSERT (p0.qos.partition.strs[2] != p0strs[2]);
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[0], p0strs[0]);
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[1], p0strs[1]);
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[2], p0strs[2]);
  memcpy (&p0memcpy, &p0, sizeof (p0));

  /* copy an aliased one; the original must be unchanged, the copy unaliased */
  nn_plist_t p3;
  nn_plist_init_empty (&p3);
  nn_plist_copy (&p3, &p0);
  CU_ASSERT (memcmp (&p0, &p0memcpy, sizeof (p0)) == 0);
  CU_ASSERT (p3.present == p0.present);
  CU_ASSERT (p3.aliased == 0);
  CU_ASSERT (p3.qos.present == p0.qos.present);
  CU_ASSERT (p3.qos.aliased == 0);
  CU_ASSERT (p3.process_id == p0.process_id);
  CU_ASSERT (p3.entity_name != p0.entity_name);
  CU_ASSERT_STRING_EQUAL (p3.entity_name, p0.entity_name);
  CU_ASSERT (p3.qos.partition.n == p0.qos.partition.n);
  CU_ASSERT (p3.qos.partition.strs != p0.qos.partition.strs);
  CU_ASSERT (p3.qos.partition.strs[0] != p0.qos.partition.strs[0]);
  CU_ASSERT (p3.qos.partition.strs[1] != p0.qos.partition.strs[1]);
  CU_ASSERT (p3.qos.partition.strs[2] != p0.qos.partition.strs[2]);
  CU_ASSERT_STRING_EQUAL (p3.qos.partition.strs[0], p0.qos.partition.strs[0]);
  CU_ASSERT_STRING_EQUAL (p3.qos.partition.strs[1], p0.qos.partition.strs[1]);
  CU_ASSERT_STRING_EQUAL (p3.qos.partition.strs[2], p0.qos.partition.strs[2]);

  /* merge-in missing ones from an aliased copy: original must remain unchanged;
     existing ones should stay without touching "aliased" only new ones are
     added as unaliased ones */
  nn_plist_t p4, p4memcpy;
  nn_plist_init_empty (&p4);
  p4.present = PP_ENTITY_NAME;
  p4.aliased = PP_ENTITY_NAME;
  p4.entity_name = "omen";
  memcpy (&p4memcpy, &p4, sizeof (p4));
  nn_plist_mergein_missing (&p4, &p0, p0.present, p0.qos.present);
  CU_ASSERT (memcmp (&p0, &p0memcpy, sizeof (p0)) == 0);
  CU_ASSERT (p4.present == p0.present);
  CU_ASSERT (p4.aliased == p4memcpy.aliased);
  CU_ASSERT (p4.qos.present == p0.qos.present);
  CU_ASSERT (p4.qos.aliased == p4memcpy.qos.aliased);
  CU_ASSERT (p4.process_id == p0.process_id);
  CU_ASSERT (p4.entity_name == p4memcpy.entity_name);
  CU_ASSERT_STRING_EQUAL (p4.entity_name, "omen");
  CU_ASSERT (p4.qos.partition.n == p0.qos.partition.n);
  CU_ASSERT (p4.qos.partition.strs != p0.qos.partition.strs);
  CU_ASSERT (p4.qos.partition.strs[0] != p0.qos.partition.strs[0]);
  CU_ASSERT (p4.qos.partition.strs[1] != p0.qos.partition.strs[1]);
  CU_ASSERT (p4.qos.partition.strs[2] != p0.qos.partition.strs[2]);
  CU_ASSERT_STRING_EQUAL (p4.qos.partition.strs[0], p0.qos.partition.strs[0]);
  CU_ASSERT_STRING_EQUAL (p4.qos.partition.strs[1], p0.qos.partition.strs[1]);
  CU_ASSERT_STRING_EQUAL (p4.qos.partition.strs[2], p0.qos.partition.strs[2]);

  nn_plist_fini (&p0);
  nn_plist_fini (&p1);
  nn_plist_fini (&p2);
  nn_plist_fini (&p3);
  nn_plist_fini (&p4);
}
