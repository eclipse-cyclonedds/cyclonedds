// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <wchar.h>

#include "CUnit/Theory.h"
#include "dds/dds.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/random.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/countargs.h"
#include "dds/ddsrt/foreach.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsc/dds_public_impl.h"
#include "dds/ddsc/dds_internal_api.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds__topic.h"
#include "test_util.h"
#include "MinXcdrVersion.h"
#include "CdrStreamOptimize.h"
#include "CdrStreamSkipDefault.h"
#include "CdrStreamKeySize.h"
#include "CdrStreamKeyExt.h"
#include "CdrStreamDataTypeInfo.h"
#include "CdrStreamChecking.h"
#include "CdrStreamWstring.h"
#include "mem_ser.h"

#define DDS_DOMAINID1 0
#define DDS_DOMAINID2 1
#define DDS_CONFIG "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"

#define _C "0123456789"
#define RND_INT16 ((int16_t) ddsrt_random ())
#define RND_INT32 ((int32_t) ddsrt_random ())
#define RND_UINT32 (ddsrt_random ())
#define RND_CHAR (_C[ddsrt_random () % 10])
#define RND_CHAR4 RND_CHAR, RND_CHAR, RND_CHAR
#define RND_CHAR8 RND_CHAR4, RND_CHAR4
#define RND_STR32 (ddsrt_random () % 2 ? (char []){ 't', RND_CHAR4, 0 } : (char []){ 't', 'e', 's', 't', RND_CHAR8, RND_CHAR8, RND_CHAR8, RND_CHAR4, 0 })
#define RND_STR5 (ddsrt_random () % 2 ? (char []){ 't', RND_CHAR, 0 } : (char []){ 't', RND_CHAR4, 0 })

#define XCDR1 DDSI_RTPS_CDR_ENC_VERSION_1
#define XCDR2 DDSI_RTPS_CDR_ENC_VERSION_2

typedef void * (*sample_empty) (void);
typedef void * (*sample_init) (void);
typedef bool (*keys_equal) (void *s1, void *s2);
typedef bool (*sample_equal) (void *s1, void *s2);
typedef void (*sample_free) (void *);
typedef void (*sample_free2) (void *, void *);

/**********************************************
 * Nested structs
 **********************************************/
typedef struct TestIdl_SubMsg1
{
  uint32_t submsg_field1;
} TestIdl_SubMsg1;

typedef struct TestIdl_SubMsg2
{
  uint32_t submsg_field1;
  uint32_t * submsg_field2;
  TestIdl_SubMsg1 submsg_field3;
} TestIdl_SubMsg2;

typedef struct TestIdl_MsgNested
{
  TestIdl_SubMsg1 msg_field1;
  TestIdl_SubMsg2 msg_field2;
  TestIdl_SubMsg1 msg_field3;
} TestIdl_MsgNested;

static const uint32_t TestIdl_MsgNested_ops [] =
{
  // Msg2
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_MsgNested, msg_field1), (3u << 16u) + 18u,  // SubMsg1
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_MsgNested, msg_field2), (3u << 16u) + 7u,   // SubMsg2
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_MsgNested, msg_field3), (3u << 16u) + 12u,  // SubMsg1
  DDS_OP_RTS,

  // SubMsg2
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsg2, submsg_field1),
  DDS_OP_ADR | DDS_OP_FLAG_EXT | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsg2, submsg_field2),
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_SubMsg2, submsg_field3), (3u << 16u) + 4u, // SubMsg2
  DDS_OP_RTS,

  // SubMsg1
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsg1, submsg_field1),
  DDS_OP_RTS
};

const dds_topic_descriptor_t TestIdl_MsgNested_desc = { sizeof (TestIdl_MsgNested), 4u, 0u, 0u, "TestIdl::MsgNested", NULL, 17, TestIdl_MsgNested_ops, "" };

static void * sample_init_nested (void)
{
  uint32_t *subf2 = ddsrt_malloc (sizeof (*subf2));
  *subf2 = RND_UINT32;
  TestIdl_MsgNested msg = {
            .msg_field1 = { .submsg_field1 = RND_UINT32 },
            .msg_field2 = { .submsg_field1 = RND_UINT32, .submsg_field2 = subf2, .submsg_field3 = { .submsg_field1 = RND_UINT32 } },
            .msg_field3 = { .submsg_field1 = RND_UINT32 } };
  return ddsrt_memdup (&msg, sizeof (TestIdl_MsgNested));
}

static bool sample_equal_nested (void *s1, void *s2)
{
  TestIdl_MsgNested *msg1 = s1, *msg2 = s2;
  return msg1->msg_field1.submsg_field1 == msg2->msg_field1.submsg_field1
    && msg1->msg_field2.submsg_field1 == msg2->msg_field2.submsg_field1
    && *msg1->msg_field2.submsg_field2 == *msg2->msg_field2.submsg_field2
    && msg1->msg_field2.submsg_field3.submsg_field1 == msg2->msg_field2.submsg_field3.submsg_field1
    && msg1->msg_field3.submsg_field1 == msg2->msg_field3.submsg_field1;
}

static void sample_free_nested (void *s)
{
  TestIdl_MsgNested *msg = s;
  ddsrt_free (msg->msg_field2.submsg_field2);
  ddsrt_free (msg);
}

/**********************************************
 * String types
 **********************************************/
typedef struct TestIdl_StrType
{
  char * str1;
  char str2[6]; // bounded (6)
  char * strseq3[2];
  char strseq4[3][6]; // bounded (6)
} TestIdl_StrType;

typedef struct TestIdl_MsgStr
{
  TestIdl_StrType msg_field1;
} TestIdl_MsgStr;

static const uint32_t TestIdl_Msg_ops [] =
{
  DDS_OP_ADR | DDS_OP_TYPE_STR, offsetof (TestIdl_MsgStr, msg_field1.str1),
  DDS_OP_ADR | DDS_OP_TYPE_BST, offsetof (TestIdl_MsgStr, msg_field1.str2), 6,
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_STR, offsetof (TestIdl_MsgStr, msg_field1.strseq3), 2,
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_BST, offsetof (TestIdl_MsgStr, msg_field1.strseq4), 3, 0, 6,
  DDS_OP_RTS
};

const dds_topic_descriptor_t TestIdl_MsgStr_desc = { sizeof (TestIdl_MsgStr), sizeof (char *), 0u, 0u, "TestIdl::MsgStr", NULL, 14, TestIdl_Msg_ops, "" };

static void * sample_init_str (void)
{
  TestIdl_MsgStr *msg = ddsrt_calloc (1, sizeof (*msg));
  msg->msg_field1.str1 = ddsrt_strdup (RND_STR32);
  ddsrt_strlcpy (msg->msg_field1.str2, RND_STR5, sizeof (msg->msg_field1.str2));

  msg->msg_field1.strseq3[0] = ddsrt_strdup (RND_STR32);
  msg->msg_field1.strseq3[1] = ddsrt_strdup (RND_STR32);

  ddsrt_strlcpy (msg->msg_field1.strseq4[0], RND_STR5, sizeof (msg->msg_field1.strseq4[0]));
  ddsrt_strlcpy (msg->msg_field1.strseq4[1], RND_STR5, sizeof (msg->msg_field1.strseq4[1]));
  ddsrt_strlcpy (msg->msg_field1.strseq4[2], RND_STR5, sizeof (msg->msg_field1.strseq4[2]));

  return msg;
}

static bool sample_equal_str (void *s1, void *s2)
{
  TestIdl_MsgStr *msg1 = s1, *msg2 = s2;
  bool eq = true;
  for (int i = 0; i < 2 && eq; i++)
    if (strcmp (msg1->msg_field1.strseq3[i], msg2->msg_field1.strseq3[i]))
      eq = false;
  for (int i = 0; i < 3 && eq; i++)
    if (strcmp (msg1->msg_field1.strseq4[i], msg2->msg_field1.strseq4[i]))
      eq = false;
  return (eq
    && !strcmp (msg1->msg_field1.str1, msg2->msg_field1.str1)
    && !strcmp (msg1->msg_field1.str2, msg2->msg_field1.str2));
}

static void sample_free_str (void *s)
{
  TestIdl_MsgStr *msg = s;
  ddsrt_free (msg->msg_field1.str1);
  ddsrt_free (msg->msg_field1.strseq3[0]);
  ddsrt_free (msg->msg_field1.strseq3[1]);
  ddsrt_free (msg);
}

/**********************************************
 * Unions
 **********************************************/
typedef enum TestIdl_Kind1
{
  TestIdl_KIND1_0,
  TestIdl_KIND1_1,
  TestIdl_KIND1_2
} TestIdl_Kind1;

typedef enum TestIdl_Kind2
{
  TestIdl_KIND2_0,
  TestIdl_KIND2_5 = 5,
  TestIdl_KIND2_6,
  TestIdl_KIND2_10 = 10
} TestIdl_Kind2;

typedef enum TestIdl_Kind3
{
  TestIdl_KIND3_0,
  TestIdl_KIND3_1,
  TestIdl_KIND3_2
} TestIdl_Kind3;

typedef struct TestIdl_Union0
{
  int32_t _d;
  union
  {
    int32_t field0_1;
    uint32_t field0_2;
  } _u;
} TestIdl_Union0;

typedef struct TestIdl_Union1
{
  TestIdl_Kind3 _d;
  union
  {
    int32_t field1;
    TestIdl_Kind2 field2;
    TestIdl_Union0 field3;
  } _u;
} TestIdl_Union1;

typedef struct TestIdl_MsgUnion
{
  TestIdl_Kind1 msg_field1;
  TestIdl_Kind2 msg_field2;
  TestIdl_Union1 msg_field3;
} TestIdl_MsgUnion;

static const uint32_t TestIdl_MsgUnion_ops [] =
{
  DDS_OP_ADR | DDS_OP_TYPE_ENU, offsetof (TestIdl_MsgUnion, msg_field1), 2u,
  DDS_OP_ADR | DDS_OP_TYPE_ENU, offsetof (TestIdl_MsgUnion, msg_field2), 10u,

  DDS_OP_ADR | DDS_OP_TYPE_UNI | DDS_OP_SUBTYPE_ENU, offsetof (TestIdl_MsgUnion, msg_field3._d), 3u, (26u << 16) + 5u, 2u,
    DDS_OP_JEQ | DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN | 0, TestIdl_KIND3_0, offsetof (TestIdl_MsgUnion, msg_field3._u.field1),
    DDS_OP_JEQ4 | DDS_OP_TYPE_ENU, TestIdl_KIND3_1, offsetof (TestIdl_MsgUnion, msg_field3._u.field2), 10u,
    DDS_OP_JEQ | DDS_OP_TYPE_UNI | 3, TestIdl_KIND3_2, offsetof (TestIdl_MsgUnion, msg_field3._u.field3),

  DDS_OP_ADR | DDS_OP_TYPE_UNI | DDS_OP_SUBTYPE_4BY | DDS_OP_FLAG_SGN, offsetof (TestIdl_Union0, _d), 2u, (10u << 16) + 4u,
    DDS_OP_JEQ | DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN | 0, 0, offsetof (TestIdl_Union0, _u.field0_1),
    DDS_OP_JEQ | DDS_OP_TYPE_4BY | 0, 1, offsetof (TestIdl_Union0, _u.field0_2),
  DDS_OP_RTS,
  DDS_OP_RTS
};

const dds_topic_descriptor_t TestIdl_MsgUnion_desc = { sizeof (TestIdl_MsgUnion), 4u, 0u, 0u, "TestIdl::MsgUnion", NULL, 3, TestIdl_MsgUnion_ops, "" };

static void * sample_init_union (void)
{
  TestIdl_MsgUnion msg = { .msg_field1 = TestIdl_KIND1_1, .msg_field2 = TestIdl_KIND2_10, .msg_field3._d = TestIdl_KIND3_1, .msg_field3._u.field2 = TestIdl_KIND2_6 };
  return ddsrt_memdup (&msg, sizeof (TestIdl_MsgUnion));
}

static bool sample_equal_union (void *s1, void *s2)
{
  TestIdl_MsgUnion *msg1 = s1, *msg2 = s2;
  return msg1->msg_field1 == msg2->msg_field1
    && msg1->msg_field2 == msg2->msg_field2
    && msg1->msg_field3._d == msg2->msg_field3._d
    && msg1->msg_field3._u.field2 == msg2->msg_field3._u.field2;
}

static void sample_free_union (void *s)
{
  ddsrt_free (s);
}

/**********************************************
 * Recursive types
 **********************************************/

struct TestIdl_SubMsgRecursive;

typedef struct TestIdl_SubMsgRecursive_submsg_field2_seq
{
  uint32_t _maximum;
  uint32_t _length;
  struct TestIdl_SubMsgRecursive *_buffer;
  bool _release;
} TestIdl_SubMsgRecursive_submsg_field2_seq;

typedef struct TestIdl_SubMsgRecursive
{
  uint32_t submsg_field1;
  TestIdl_SubMsgRecursive_submsg_field2_seq submsg_field2;
  int32_t submsg_field3;
} TestIdl_SubMsgRecursive;

typedef struct TestIdl_MsgRecursive
{
  uint32_t msg_field1;
  TestIdl_SubMsgRecursive msg_field2;
  int32_t msg_field3;
} TestIdl_MsgRecursive;

static const uint32_t TestIdl_MsgRecursive_ops [] =
{
  // MsgRecursive
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_MsgRecursive, msg_field1),
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_MsgRecursive, msg_field2), (3u << 16u) + 6u,   // SubMsg
  DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN, offsetof (TestIdl_MsgRecursive, msg_field3),
  DDS_OP_RTS,

  // SubMsgRecursive
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsgRecursive, submsg_field1),
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU, offsetof (TestIdl_SubMsgRecursive, submsg_field2), sizeof (TestIdl_SubMsgRecursive), (6u << 16u) + 4u,   // sequence<SubMsgRecursive>
    DDS_OP_JSR | (65536 - 6), DDS_OP_RTS,   // SubMsgRecursive
  DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN, offsetof (TestIdl_SubMsgRecursive, submsg_field3),
  DDS_OP_RTS
};

const dds_topic_descriptor_t TestIdl_MsgRecursive_desc = { sizeof (TestIdl_MsgRecursive), 4u, 0u, 0u, "TestIdl::MsgRecursive", NULL, 18, TestIdl_MsgRecursive_ops, "" };

static void * sample_init_recursive (void)
{
  uint32_t sseqsz = RND_UINT32 % 1024;
  TestIdl_SubMsgRecursive *sseq = ddsrt_malloc (sseqsz * sizeof (*sseq));
  for (uint32_t n = 0; n < sseqsz; n++)
    sseq[n] = (TestIdl_SubMsgRecursive) { .submsg_field1 = RND_UINT32, .submsg_field3 = RND_INT32 };
  TestIdl_SubMsgRecursive_submsg_field2_seq seq = { ._length = sseqsz, ._maximum = sseqsz, ._buffer = sseq };
  TestIdl_SubMsgRecursive s1 = { .submsg_field1 = RND_UINT32, .submsg_field2 = seq, .submsg_field3 = RND_INT32 };
  TestIdl_MsgRecursive msg = { .msg_field1 = RND_UINT32, .msg_field2 = s1, .msg_field3 = RND_INT32};
  return ddsrt_memdup (&msg, sizeof (TestIdl_MsgRecursive));
}

static bool sample_equal_recursive_field2 (TestIdl_SubMsgRecursive *s1, TestIdl_SubMsgRecursive *s2)
{
  if (s1->submsg_field1 != s2->submsg_field1
    || s1->submsg_field2._length != s2->submsg_field2._length
    || s1->submsg_field3 != s2->submsg_field3)
    return false;

  for (uint32_t n = 0; n < s1->submsg_field2._length; n++)
    if (!sample_equal_recursive_field2 (&s1->submsg_field2._buffer[n], &s2->submsg_field2._buffer[n]))
      return false;

  return true;
}

static bool sample_equal_recursive (void *s1, void *s2)
{
  TestIdl_MsgRecursive *msg1 = s1, *msg2 = s2;
  return (msg1->msg_field1 == msg2->msg_field1
    && sample_equal_recursive_field2 (&msg1->msg_field2, &msg2->msg_field2)
    && msg1->msg_field3 == msg2->msg_field3);
}

static void sample_free_recursive (void *s)
{
  TestIdl_MsgRecursive *msg = s;
  ddsrt_free (msg->msg_field2.submsg_field2._buffer);
  ddsrt_free (s);
}

/**********************************************
 * External fields
 **********************************************/
typedef struct TestIdl_MsgExt_b
{
  int32_t * b1;
} TestIdl_MsgExt_b;

typedef struct dds_sequence_short
{
  uint32_t _maximum;
  uint32_t _length;
  int16_t *_buffer;
  bool _release;
} dds_sequence_short;

typedef struct dds_sequence_TestIdl_MsgExt_b
{
  uint32_t _maximum;
  uint32_t _length;
  struct TestIdl_MsgExt_b *_buffer;
  bool _release;
} dds_sequence_TestIdl_MsgExt_b;

typedef struct TestIdl_MsgExt
{
  char * f1;
  char (* f2)[33];
  struct TestIdl_MsgExt_b * f3;
  int16_t (* f4)[3];
  dds_sequence_short * f5;
  dds_sequence_TestIdl_MsgExt_b * f6;
} TestIdl_MsgExt;

static const uint32_t TestIdl_MsgExt_ops [] =
{
  /* TestIdl_MsgExt */
  DDS_OP_ADR | DDS_OP_TYPE_STR, offsetof (TestIdl_MsgExt, f1),
  DDS_OP_ADR | DDS_OP_FLAG_EXT | DDS_OP_TYPE_BST, offsetof (TestIdl_MsgExt, f2), 33u,
  DDS_OP_ADR | DDS_OP_FLAG_EXT | DDS_OP_TYPE_EXT, offsetof (TestIdl_MsgExt, f3), (4u << 16u) + 14u /* TestIdl_MsgExt_b */, sizeof (TestIdl_MsgExt_b),
  DDS_OP_ADR | DDS_OP_FLAG_EXT | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_2BY | DDS_OP_FLAG_SGN, offsetof (TestIdl_MsgExt, f4), 3u,
  DDS_OP_ADR | DDS_OP_FLAG_EXT | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_2BY | DDS_OP_FLAG_SGN, offsetof (TestIdl_MsgExt, f5),
  DDS_OP_ADR | DDS_OP_FLAG_EXT | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU, offsetof (TestIdl_MsgExt, f6), sizeof (TestIdl_MsgExt_b), (4u << 16u) + 5u /* TestIdl_MsgExt_b */,
  DDS_OP_RTS,

  /* TestIdl_MsgExt_b */
  DDS_OP_ADR | DDS_OP_FLAG_EXT | DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN, offsetof (TestIdl_MsgExt_b, b1),
  DDS_OP_RTS
};

const dds_topic_descriptor_t TestIdl_MsgExt_desc = { sizeof (TestIdl_MsgExt), sizeof (char *), 0u, 0u, "TestIdl_MsgExt", NULL, 9, TestIdl_MsgExt_ops, "" };

static void * sample_init_ext (void)
{
  TestIdl_MsgExt *msg = ddsrt_malloc (sizeof (*msg));
  msg->f1 = ddsrt_strdup (RND_STR32);

  msg->f2 = ddsrt_malloc (sizeof (*msg->f2));
  ddsrt_strlcpy (*msg->f2, RND_STR32, sizeof (*msg->f2));

  msg->f3 = ddsrt_malloc (sizeof (*msg->f3));
  msg->f3->b1 = ddsrt_malloc (sizeof (*msg->f3->b1));
  *msg->f3->b1 = RND_INT32;

  msg->f4 = ddsrt_malloc (sizeof (*msg->f4));
  (*msg->f4)[0] = RND_INT16;
  (*msg->f4)[1] = RND_INT16;
  (*msg->f4)[2] = RND_INT16;

  msg->f5 = ddsrt_malloc (sizeof (*msg->f5));
  uint32_t seqsz5 = RND_UINT32 % 255;
  msg->f5->_length = msg->f5->_maximum = seqsz5;
  msg->f5->_buffer = ddsrt_malloc (seqsz5 * sizeof (*msg->f5->_buffer));
  msg->f5->_release = true;
  for (uint32_t n = 0; n < seqsz5; n++)
    msg->f5->_buffer[n] = RND_INT16;

  msg->f6 = ddsrt_malloc (sizeof (*msg->f6));
  uint32_t seqsz6 = RND_UINT32 % 255;
  msg->f6->_length = msg->f6->_maximum = seqsz6;
  msg->f6->_buffer = ddsrt_malloc (seqsz6 * sizeof (*msg->f6->_buffer));
  msg->f6->_release = true;
  for (uint32_t n = 0; n < seqsz6; n++)
  {
    msg->f6->_buffer[n].b1 = ddsrt_malloc (sizeof (*msg->f6->_buffer[n].b1));
    *msg->f6->_buffer[n].b1 = RND_INT32;
  }

  return msg;
}

static bool sample_equal_ext (void *s1, void *s2)
{
  TestIdl_MsgExt *msg1 = s1, *msg2 = s2;

  if (msg1->f5->_length != msg2->f5->_length)
    return false;
  for (uint32_t n = 0; n < msg1->f5->_length; n++)
    if (msg1->f5->_buffer[n] != msg2->f5->_buffer[n])
      return false;

  if (msg1->f6->_length != msg2->f6->_length)
    return false;
  for (uint32_t n = 0; n < msg1->f6->_length; n++)
    if (*msg1->f6->_buffer[n].b1 != *msg2->f6->_buffer[n].b1)
      return false;

  return
    !strcmp (msg1->f1, msg2->f1)
    && !strcmp (*msg1->f2, *msg2->f2)
    && *msg1->f3->b1 == *msg2->f3->b1
    && (*msg1->f4)[0] == (*msg2->f4)[0]
    && (*msg1->f4)[1] == (*msg2->f4)[1]
    && (*msg1->f4)[2] == (*msg2->f4)[2];
}

static void sample_free_ext (void *s)
{
  dds_stream_free_sample (s, &dds_cdrstream_default_allocator, TestIdl_MsgExt_desc.m_ops);
  ddsrt_free (s);
}

/**********************************************
 * External fields
 **********************************************/

typedef struct TestIdl_MsgOpt_b
{
  bool * b1;
} TestIdl_MsgOpt_b;

typedef struct TestIdl_MsgOpt_dds_sequence_b
{
  uint32_t _maximum;
  uint32_t _length;
  struct TestIdl_MsgOpt_b *_buffer;
  bool _release;
} TestIdl_MsgOpt_dds_sequence_b;

typedef struct TestIdl_MsgOpt
{
  int32_t * f1;
  char * f2;
  struct TestIdl_MsgOpt_b * f3;
  char (* f4)[33];
  int32_t (* f5)[3];
  TestIdl_MsgOpt_dds_sequence_b * f6;
} TestIdl_MsgOpt;

static const uint32_t TestIdl_MsgOpt_ops [] =
{
  /* TestIdl_MsgOpt */
  DDS_OP_ADR | DDS_OP_FLAG_EXT | DDS_OP_TYPE_4BY | DDS_OP_FLAG_OPT | DDS_OP_FLAG_SGN, offsetof (TestIdl_MsgOpt, f1),
  DDS_OP_ADR | DDS_OP_TYPE_STR | DDS_OP_FLAG_OPT, offsetof (TestIdl_MsgOpt, f2),
  DDS_OP_ADR | DDS_OP_FLAG_EXT | DDS_OP_TYPE_EXT | DDS_OP_FLAG_OPT, offsetof (TestIdl_MsgOpt, f3), (4u << 16u) + 15u /* TestIdl_MsgOpt_b */, sizeof (TestIdl_MsgOpt_b),
  DDS_OP_ADR | DDS_OP_FLAG_EXT | DDS_OP_TYPE_BST | DDS_OP_FLAG_OPT, offsetof (TestIdl_MsgOpt, f4), 33u,
  DDS_OP_ADR | DDS_OP_FLAG_EXT | DDS_OP_TYPE_ARR | DDS_OP_FLAG_OPT | DDS_OP_SUBTYPE_4BY | DDS_OP_FLAG_SGN, offsetof (TestIdl_MsgOpt, f5), 3u,
  DDS_OP_ADR | DDS_OP_FLAG_EXT | DDS_OP_TYPE_SEQ | DDS_OP_FLAG_OPT | DDS_OP_SUBTYPE_STU, offsetof (TestIdl_MsgOpt, f6), sizeof (TestIdl_MsgOpt_b), (4u << 16u) + 5u /* TestIdl_MsgOpt_b */,
  DDS_OP_RTS,

  /* TestIdl_MsgOpt_b */
  DDS_OP_ADR | DDS_OP_FLAG_EXT | DDS_OP_TYPE_1BY | DDS_OP_FLAG_OPT, offsetof (TestIdl_MsgOpt_b, b1),
  DDS_OP_RTS
};

const dds_topic_descriptor_t TestIdl_MsgOpt_desc = { sizeof (TestIdl_MsgOpt), sizeof (char *), 0u, 0u, "TestIdl_MsgOpt", NULL, 9, TestIdl_MsgOpt_ops, "" };

static void * sample_init_opt (void)
{
  TestIdl_MsgOpt *msg = ddsrt_calloc (1, sizeof (*msg));

  if (RND_INT32 % 2)
  {
    msg->f1 = ddsrt_malloc (sizeof (*msg->f1));
    *msg->f1 = RND_INT32;
  }
  if (RND_INT32 % 2)
    msg->f2 = ddsrt_strdup (RND_STR32);
  if (RND_INT32 % 2)
  {
    msg->f3 = ddsrt_calloc (1, sizeof (*msg->f3));
    if (RND_INT32 % 2)
    {
      msg->f3->b1 = ddsrt_malloc (sizeof (*msg->f3->b1));
      *msg->f3->b1 = (RND_INT32 % 2);
    }
  }
  if (RND_INT32 % 2)
  {
    msg->f4 = ddsrt_malloc (sizeof (*msg->f4));
    ddsrt_strlcpy (*msg->f4, RND_STR32, sizeof (*msg->f4));
  }
  if (RND_INT32 % 2)
  {
    msg->f5 = ddsrt_malloc (sizeof (*msg->f5));
    (*msg->f5)[0] = RND_INT32;
    (*msg->f5)[1] = RND_INT32;
    (*msg->f5)[2] = RND_INT32;
  }
  if (RND_INT32 % 2)
  {
    msg->f6 = ddsrt_malloc (sizeof (*msg->f6));
    uint32_t seqsz6 = RND_UINT32 % 255;
    msg->f6->_length = msg->f6->_maximum = seqsz6;
    msg->f6->_buffer = ddsrt_calloc (seqsz6, sizeof (*msg->f6->_buffer));
    msg->f6->_release = true;
    for (uint32_t n = 0; n < seqsz6; n++)
    {
      if (RND_INT32 % 2)
      {
        msg->f6->_buffer[n].b1 = ddsrt_malloc (sizeof (*msg->f6->_buffer[n].b1));
        *msg->f6->_buffer[n].b1 = RND_INT32;
      }
    }
  }
  return msg;
}

static bool sample_equal_opt (void *s1, void *s2)
{
  TestIdl_MsgOpt *msg1 = s1, *msg2 = s2;

  if (((msg1->f1 == NULL && msg2->f1 != NULL) || (msg1->f1 != NULL && msg2->f1 == NULL))
      || ((msg1->f2 == NULL && msg2->f2 != NULL) || (msg1->f2 != NULL && msg2->f2 == NULL))
      || ((msg1->f3 == NULL && msg2->f3 != NULL) || (msg1->f3 != NULL && msg2->f3 == NULL))
      || (msg1->f3 != NULL && ((msg1->f3->b1 == NULL && msg2->f3->b1 != NULL) || (msg1->f3->b1 != NULL && msg2->f3->b1 == NULL)))
      || ((msg1->f4 == NULL && msg2->f4 != NULL) || (msg1->f4 != NULL && msg2->f4 == NULL))
      || ((msg1->f5 == NULL && msg2->f5 != NULL) || (msg1->f5 != NULL && msg2->f5 == NULL))
      || ((msg1->f6 == NULL && msg2->f6 != NULL) || (msg1->f6 != NULL && msg2->f6 == NULL)))
    return false;

  if (msg1->f6 != NULL)
  {
    if (msg1->f6->_length != msg2->f6->_length)
      return false;
    for (uint32_t n = 0; n < msg1->f6->_length; n++)
    {
      if ((msg1->f6->_buffer[n].b1 == NULL && msg2->f6->_buffer[n].b1 != NULL) || (msg1->f6->_buffer[n].b1 != NULL && msg2->f6->_buffer[n].b1 == NULL))
        return false;
      if (msg1->f6->_buffer[n].b1 != NULL && *msg1->f6->_buffer[n].b1 != *msg2->f6->_buffer[n].b1)
        return false;
    }
  }

  return
    (msg1->f1 == NULL || *msg1->f1 == *msg2->f1)
    && (msg1->f2 == NULL || !strcmp (msg1->f2, msg2->f2))
    && (msg1->f3 == NULL || msg1->f3->b1 == NULL || *msg1->f3->b1 == *msg2->f3->b1)
    && (msg1->f4 == NULL || !strcmp (*msg1->f4, *msg2->f4))
    && (msg1->f5 == NULL || ((*msg1->f5)[0] == (*msg2->f5)[0] && (*msg1->f5)[1] == (*msg2->f5)[1] && (*msg1->f5)[2] == (*msg2->f5)[2]));
}

static void sample_free_opt (void *s)
{
  dds_stream_free_sample (s, &dds_cdrstream_default_allocator, TestIdl_MsgOpt_desc.m_ops);
  ddsrt_free (s);
}

/**********************************************
 * Appendable types
 **********************************************/

/* @appendable */
typedef struct TestIdl_AppendableUnion0
{
  int8_t _d;
  union
  {
    uint32_t field1;
    int32_t field2;
  } _u;
} TestIdl_AppendableUnion0;

/* @appendable */
typedef struct TestIdl_AppendableSubMsg1
{
  uint32_t submsg1_field1;
  char *submsg1_field2;
} TestIdl_AppendableSubMsg1;

/* @appendable */
typedef struct TestIdl_AppendableSubMsg2
{
  uint32_t submsg2_field1;
  uint32_t submsg2_field2;
} TestIdl_AppendableSubMsg2;

/* @appendable */
typedef struct TestIdl_AppendableMsg_msg_field3_seq
{
  uint32_t _maximum;
  uint32_t _length;
  struct TestIdl_AppendableSubMsg2 *_buffer;
  bool _release;
} TestIdl_AppendableMsg_msg_field3_seq;

/* @appendable */
typedef struct TestIdl_AppendableMsg_msg_field5_seq
{
  uint32_t _maximum;
  uint32_t _length;
  struct TestIdl_AppendableUnion0 *_buffer;
  bool _release;
} TestIdl_AppendableMsg_msg_field5_seq;

/* @appendable */
typedef struct TestIdl_AppendableMsg
{
  TestIdl_AppendableSubMsg1 msg_field1;
  TestIdl_AppendableSubMsg2 msg_field2;
  TestIdl_AppendableMsg_msg_field3_seq msg_field3;
  TestIdl_AppendableUnion0 msg_field4;
  TestIdl_AppendableMsg_msg_field5_seq msg_field5;
} TestIdl_AppendableMsg;

static const uint32_t TestIdl_AppendableMsg_ops [] =
{
  /* AppendableMsg */
  DDS_OP_DLC,
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_AppendableMsg, msg_field1), (3u << 16u) + 18u,  // AppendableSubMsg1
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_AppendableMsg, msg_field2), (3u << 16u) + 21u, // AppendableSubMsg2
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU , offsetof (TestIdl_AppendableMsg, msg_field3), sizeof (TestIdl_AppendableSubMsg2), (4u << 16u) + 18u,  // sequence<AppendableSubMsg2>
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_AppendableMsg, msg_field4), (3u << 16u) + 20u,  // AppendableUnion0
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_UNI, offsetof (TestIdl_AppendableMsg, msg_field5), sizeof (TestIdl_AppendableUnion0), (4u << 16u) + 17u,   // sequenec<AppendableUnion0>
  DDS_OP_RTS,

  /* AppendableSubMsg1 */
  DDS_OP_DLC,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_AppendableSubMsg1, submsg1_field1),
  DDS_OP_ADR | DDS_OP_TYPE_STR, offsetof (TestIdl_AppendableSubMsg1, submsg1_field2),
  DDS_OP_RTS,

  /* AppendableSubMsg2 */
  DDS_OP_DLC,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_AppendableSubMsg2, submsg2_field1),
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_AppendableSubMsg2, submsg2_field2),
  DDS_OP_RTS,

  /* AppendableUnion0 */
  DDS_OP_DLC,
  DDS_OP_ADR | DDS_OP_TYPE_UNI | DDS_OP_SUBTYPE_1BY, offsetof (TestIdl_AppendableUnion0, _d), 2u, (10u << 16) + 4u,
    DDS_OP_JEQ | DDS_OP_TYPE_4BY | 0, 0, offsetof (TestIdl_AppendableUnion0, _u.field1),
    DDS_OP_JEQ | DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN | 0, 1, offsetof (TestIdl_AppendableUnion0, _u.field2),
  DDS_OP_RTS,
};

const dds_topic_descriptor_t TestIdl_MsgAppendable_desc = { sizeof (TestIdl_AppendableMsg), 4u, 0u, 0u, "TestIdl::AppendableMsg", NULL, 4, TestIdl_AppendableMsg_ops, "" };

static void * sample_init_appendable (void)
{
  TestIdl_AppendableSubMsg2 sseq[] = { { .submsg2_field1 = 111, .submsg2_field2 = 222 }, { .submsg2_field1 = 333, .submsg2_field2 = 444 } };
  TestIdl_AppendableUnion0 useq[] = { { ._d = 0, ._u.field1 = 555 }, { ._d = 1, ._u.field2 = -555 }, { ._d = 0, ._u.field1 = 666 } };
  TestIdl_AppendableMsg msg = {
          .msg_field1 = { .submsg1_field1 = 1100, .submsg1_field2 = "test0123" },
          .msg_field2 = { .submsg2_field1 = 2100, .submsg2_field2 = 2200 },
          .msg_field3 = { ._length = 2, ._maximum = 2, ._buffer = ddsrt_memdup (sseq, 2 * sizeof (TestIdl_AppendableSubMsg2)) },
          .msg_field4 = { ._d = 1, ._u.field2 = -10 },
          .msg_field5 = { ._length = 3, ._maximum = 3, ._buffer = ddsrt_memdup (useq, 3 * sizeof (TestIdl_AppendableUnion0)) }
  };
  return ddsrt_memdup (&msg, sizeof (TestIdl_AppendableMsg));
}

static bool sample_equal_appendable_TestIdl_AppendableSubMsg2_seq (TestIdl_AppendableMsg_msg_field3_seq *s1, TestIdl_AppendableMsg_msg_field3_seq *s2)
{
  if (s1->_length != s2->_length)
    return false;
  for (uint32_t n = 0; n < s1->_length; n++)
  {
    if (s1->_buffer[n].submsg2_field1 != s2->_buffer[n].submsg2_field1
      || s1->_buffer[n].submsg2_field2 != s2->_buffer[n].submsg2_field2)
      return false;
  }
  return true;
}

static bool sample_equal_appendable_TestIdl_AppendableUnion0_seq (TestIdl_AppendableMsg_msg_field5_seq *s1, TestIdl_AppendableMsg_msg_field5_seq *s2)
{
  if (s1->_length != s2->_length)
    return false;
  for (uint32_t n = 0; n < s1->_length; n++)
  {
    if (s1->_buffer[n]._d != s2->_buffer[n]._d
      || s1->_buffer[n]._u.field1 != s2->_buffer[n]._u.field1)
      return false;
  }
  return true;
}

static bool sample_equal_appendable (void *s1, void *s2)
{
  TestIdl_AppendableMsg *msg1 = (TestIdl_AppendableMsg *) s1, *msg2 = (TestIdl_AppendableMsg *) s2;
  return (
    msg1->msg_field1.submsg1_field1 == msg2->msg_field1.submsg1_field1
    && !strcmp (msg1->msg_field1.submsg1_field2, msg2->msg_field1.submsg1_field2)
    && msg1->msg_field2.submsg2_field1 == msg2->msg_field2.submsg2_field1
    && msg1->msg_field2.submsg2_field2 == msg2->msg_field2.submsg2_field2
    && sample_equal_appendable_TestIdl_AppendableSubMsg2_seq (&msg1->msg_field3, &msg2->msg_field3)
    && msg1->msg_field4._d == msg2->msg_field4._d
    && msg1->msg_field4._u.field2 == msg2->msg_field4._u.field2
    && sample_equal_appendable_TestIdl_AppendableUnion0_seq (&msg1->msg_field5, &msg2->msg_field5)
  );
}

static void sample_free_appendable (void *s)
{
  TestIdl_AppendableMsg *msg = (TestIdl_AppendableMsg *) s;
  ddsrt_free (msg->msg_field3._buffer);
  ddsrt_free (msg->msg_field5._buffer);
  ddsrt_free (s);
}

/**********************************************
 * Keys in nested (appendable/mutable) types
 **********************************************/

/* @mutable */
typedef struct TestIdl_SubMsgKeysNested2
{
  uint32_t submsg2_field1;
  uint32_t submsg2_field2;
} TestIdl_SubMsgKeysNested2;

/* @appendable */
typedef struct TestIdl_SubMsgKeysNested
{
  uint32_t submsg_field1;
  uint32_t submsg_field2;
  uint32_t submsg_field3;
  TestIdl_SubMsgKeysNested2 submsg_field4;
} TestIdl_SubMsgKeysNested;

typedef struct TestIdl_MsgKeysNested_msg_field2_seq
{
  uint32_t _maximum;
  uint32_t _length;
  TestIdl_SubMsgKeysNested *_buffer;
  bool _release;
} TestIdl_MsgKeysNested_msg_field2_seq;

/* @final */
typedef struct TestIdl_MsgKeysNested
{
  TestIdl_SubMsgKeysNested msg_field1;
  TestIdl_MsgKeysNested_msg_field2_seq msg_field2;
} TestIdl_MsgKeysNested;

static const uint32_t TestIdl_MsgKeysNested_ops [] =
{
  // Msg
  DDS_OP_ADR | DDS_OP_TYPE_EXT | DDS_OP_FLAG_KEY, offsetof (TestIdl_MsgKeysNested, msg_field1), (3u << 16u) + 8u,  // SubMsgKeysNested
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU, offsetof (TestIdl_MsgKeysNested, msg_field2), sizeof (TestIdl_SubMsgKeysNested), (4u << 16u) + 5u, // sequence<SubMsgKeysNested>
  DDS_OP_RTS,

  // SubMsg
  DDS_OP_DLC,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsgKeysNested, submsg_field1),
  DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_KEY, offsetof (TestIdl_SubMsgKeysNested, submsg_field2),
  DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_KEY, offsetof (TestIdl_SubMsgKeysNested, submsg_field3),
  DDS_OP_ADR | DDS_OP_TYPE_EXT | DDS_OP_FLAG_KEY, offsetof (TestIdl_SubMsgKeysNested, submsg_field4), (3u << 16) + 4u, // SubMsgKeysNested2
  DDS_OP_RTS,

  // SubMsg2
  DDS_OP_PLC,
    DDS_OP_PLM | 5u, 1,
    DDS_OP_PLM | 6u, 2,
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsgKeysNested2, submsg2_field1),
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_KEY, offsetof (TestIdl_SubMsgKeysNested2, submsg2_field2),
  DDS_OP_RTS,

  DDS_OP_KOF | 2u, 0u, 3u,      // msg_field1.submsg_field2
  DDS_OP_KOF | 2u, 0u, 5u,      // msg_field1.submsg_field3
  DDS_OP_KOF | 3u, 0u, 7u, 9u   // msg_field1.submsg_field4.submsg2_field2
};

static const dds_key_descriptor_t TestIdl_MsgKeysNested_keys[3] =
{
  { "msg_field1.submsg_field2", 31, 0 },
  { "msg_field1.submsg_field3", 34, 1 },
  { "msg_field1.submsg_field4.submsg2_field2", 37, 2 }
};

const dds_topic_descriptor_t TestIdl_MsgKeysNested_desc = { sizeof (TestIdl_MsgKeysNested), sizeof (char *), 0u, 3u, "TestIdl::MsgKeysNested", TestIdl_MsgKeysNested_keys, 8, TestIdl_MsgKeysNested_ops, "" };

static void * sample_empty_keysnested (void)
{
  TestIdl_MsgKeysNested *msg = ddsrt_calloc (1, sizeof (*msg));
  return msg;
}

static void * sample_init_keysnested (void)
{
  TestIdl_SubMsgKeysNested sseq[] = { { .submsg_field1 = 2100, .submsg_field2 = 2200, .submsg_field3 = 2300, .submsg_field4.submsg2_field1 = 2310, .submsg_field4.submsg2_field2 = 2320 },
      { .submsg_field1 = 2101, .submsg_field2 = 2201, .submsg_field3 = 2301, .submsg_field4.submsg2_field1 = 2411, .submsg_field4.submsg2_field2 = 2412 } };
  TestIdl_MsgKeysNested msg = {
          .msg_field1 = { .submsg_field1 = 1100, .submsg_field2 = 1200, .submsg_field3 = 1300, .submsg_field4.submsg2_field1 = 1410, .submsg_field4.submsg2_field2 = 1420 },
          .msg_field2 = { ._length = 2, ._maximum = 2, ._buffer = ddsrt_memdup (sseq, 2 * sizeof (TestIdl_SubMsgKeysNested)) }
  };
  return ddsrt_memdup (&msg, sizeof (TestIdl_MsgKeysNested));
}

static bool keys_equal_keysnested (void *s1, void *s2)
{
  TestIdl_MsgKeysNested *msg1 = (TestIdl_MsgKeysNested *) s1, *msg2 = (TestIdl_MsgKeysNested *) s2;
  return
      msg1->msg_field1.submsg_field2 == msg2->msg_field1.submsg_field2
      && msg1->msg_field1.submsg_field3 == msg2->msg_field1.submsg_field3
      && msg1->msg_field1.submsg_field4.submsg2_field2 == msg2->msg_field1.submsg_field4.submsg2_field2;
}

static bool sample_equal_keysnested_TestIdl_SubMsgKeysNested (TestIdl_SubMsgKeysNested *s1, TestIdl_SubMsgKeysNested *s2)
{
  return (s1->submsg_field1 == s2->submsg_field1
    && s1->submsg_field2 == s2->submsg_field2
    && s1->submsg_field3 == s2->submsg_field3
    && s1->submsg_field4.submsg2_field1 == s2->submsg_field4.submsg2_field1
    && s1->submsg_field4.submsg2_field2 == s2->submsg_field4.submsg2_field2);
}

static bool sample_equal_keysnested (void *s1, void *s2)
{
  TestIdl_MsgKeysNested *msg1 = (TestIdl_MsgKeysNested *) s1, *msg2 = (TestIdl_MsgKeysNested *) s2;
  if (!sample_equal_keysnested_TestIdl_SubMsgKeysNested (&msg1->msg_field1, &msg2->msg_field1)
    || msg1->msg_field2._length != msg2->msg_field2._length)
    return false;
  for (uint32_t n = 0; n < msg1->msg_field2._length; n++)
  {
    if (!sample_equal_keysnested_TestIdl_SubMsgKeysNested (&msg1->msg_field2._buffer[n], &msg2->msg_field2._buffer[n]))
      return false;
  }
  return true;
}

static void sample_free_keysnested (void *s)
{
  TestIdl_MsgKeysNested *msg = (TestIdl_MsgKeysNested *) s;
  ddsrt_free (msg->msg_field2._buffer);
  ddsrt_free (s);
}

/**********************************************
 * Arrays
 **********************************************/
typedef struct TestIdl_SubMsgArr
{
  int32_t field1;
  int32_t field2;
} TestIdl_SubMsgArr;

typedef struct TestIdl_UnionArr
{
  int32_t _d;
  union
  {
    int32_t union_field1;
    uint32_t union_field2;
  } _u;
} TestIdl_UnionArr;

typedef struct TestIdl_MsgArr
{
   int32_t msg_field1[2];
   TestIdl_SubMsgArr msg_field2[2];
   TestIdl_UnionArr msg_field3[2];
} TestIdl_MsgArr;

static const uint32_t TestIdl_MsgArr_ops [] =
{
  /* TestIdl_MsgArr */
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_4BY, offsetof (TestIdl_MsgArr, msg_field1), 2u,
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_STU, offsetof (TestIdl_MsgArr, msg_field2), 2u, (5u << 16) + 11u, sizeof (TestIdl_SubMsgArr),
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_UNI, offsetof (TestIdl_MsgArr, msg_field3), 2u, (5u << 16) + 11u, sizeof (TestIdl_UnionArr),
  DDS_OP_RTS,

  /* TestIdl_SubMsgArr */
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsgArr, field1),
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsgArr, field2),
  DDS_OP_RTS,

  /* TestIdl_UnionArr */
  DDS_OP_ADR | DDS_OP_TYPE_UNI | DDS_OP_SUBTYPE_4BY | DDS_OP_FLAG_SGN, offsetof (TestIdl_UnionArr, _d), 2u, (10u << 16) + 4u,
    DDS_OP_JEQ | DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN | 0, 0, offsetof (TestIdl_UnionArr, _u.union_field1),
    DDS_OP_JEQ | DDS_OP_TYPE_4BY | 0, 1, offsetof (TestIdl_UnionArr, _u.union_field2),
  DDS_OP_RTS
};

const dds_topic_descriptor_t TestIdl_MsgArr_desc = { sizeof (TestIdl_MsgArr), sizeof (char *), 0u, 0u, "TestIdl::MsgArr", NULL, 6, TestIdl_MsgArr_ops, "" };

static void * sample_init_arr (void)
{
  TestIdl_MsgArr msg = {
    .msg_field1 = { 1, 2 },
    .msg_field2 = { { .field1 = 111, .field2 = 222 }, { .field1 = 333, .field2 = 444 } },
    .msg_field3 = { { ._d = 0, ._u.union_field1 = 1 }, { ._d = 1, ._u.union_field2 = 2 } }
  };
  return ddsrt_memdup (&msg, sizeof (TestIdl_MsgArr));
}

static bool sample_equal_arr (void *s1, void *s2)
{
  TestIdl_MsgArr *msg1 = s1, *msg2 = s2;
  return (msg1->msg_field1[0] == msg2->msg_field1[0]
    && msg1->msg_field1[1] == msg2->msg_field1[1]
    && msg1->msg_field2[0].field1 == msg2->msg_field2[0].field1
    && msg1->msg_field2[0].field2 == msg2->msg_field2[0].field2
    && msg1->msg_field2[1].field1 == msg2->msg_field2[1].field1
    && msg1->msg_field2[1].field2 == msg2->msg_field2[1].field2
    && msg1->msg_field3[0]._d == msg2->msg_field3[0]._d
    && msg1->msg_field3[0]._u.union_field1 == msg2->msg_field3[0]._u.union_field1
    && msg1->msg_field3[1]._d == msg2->msg_field3[1]._d
    && msg1->msg_field3[1]._u.union_field2 == msg2->msg_field3[1]._u.union_field2
  );
}

static void sample_free_arr (void *s)
{
  TestIdl_MsgArr *msg = s;
  ddsrt_free (msg);
}

/**********************************************
 * Appendable types: structs
 **********************************************/

/* @appendable */
typedef struct TestIdl_SubMsgAppendStruct1
{
  uint32_t submsg_field1;
  uint32_t submsg_field2;
} TestIdl_SubMsgAppendStruct1;

/* @appendable */
typedef struct TestIdl_MsgAppendStruct1
{
  uint32_t msg_field1;
  TestIdl_SubMsgAppendStruct1 msg_field2;
  uint32_t msg_field3;
} TestIdl_MsgAppendStruct1;

static const uint32_t TestIdl_MsgAppendStruct1_ops [] =
{
  DDS_OP_DLC,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_MsgAppendStruct1, msg_field1),
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_MsgAppendStruct1, msg_field2), (3u << 16u) + 6u,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_MsgAppendStruct1, msg_field3),
  DDS_OP_RTS,

  DDS_OP_DLC,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsgAppendStruct1, submsg_field1),
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsgAppendStruct1, submsg_field2),
  DDS_OP_RTS
};

/* @appendable */
typedef struct TestIdl_SubMsgAppendStruct2
{
  uint32_t submsg_field1;
  uint32_t submsg_field2;
  uint32_t submsg_field3[10000];
} TestIdl_SubMsgAppendStruct2;

/* @appendable */
typedef struct TestIdl_MsgAppendStruct2
{
  uint32_t msg_field1;
  TestIdl_SubMsgAppendStruct2 msg_field2;
  uint32_t msg_field3;
  uint32_t msg_field4[10000];
} TestIdl_MsgAppendStruct2;

static const uint32_t TestIdl_MsgAppendStruct2_ops [] =
{
  DDS_OP_DLC,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_MsgAppendStruct2, msg_field1),
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_MsgAppendStruct2, msg_field2), (3u << 16u) + 9u,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_MsgAppendStruct2, msg_field3),
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_4BY, offsetof (TestIdl_MsgAppendStruct2, msg_field4), 10000u,
  DDS_OP_RTS,

  DDS_OP_DLC,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsgAppendStruct2, submsg_field1),
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsgAppendStruct2, submsg_field2),
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_4BY, offsetof (TestIdl_SubMsgAppendStruct2, submsg_field3), 10000u,
  DDS_OP_RTS
};

const dds_topic_descriptor_t TestIdl_MsgAppendStruct1_desc = { sizeof (TestIdl_MsgAppendStruct1), 4u, 0u, 0u, "TestIdl::MsgAppendStruct1", NULL, 0, TestIdl_MsgAppendStruct1_ops, "" };
const dds_topic_descriptor_t TestIdl_MsgAppendStruct2_desc = { sizeof (TestIdl_MsgAppendStruct2), 4u, 0u, 0u, "TestIdl::MsgAppendStruct2", NULL, 0, TestIdl_MsgAppendStruct2_ops, "" };

static void * sample_init_appendstruct1 (void)
{
  TestIdl_MsgAppendStruct1 msg = { .msg_field1 = 1, .msg_field2 = { .submsg_field1 = 11, .submsg_field2 = 22 }, .msg_field3 = 3 };
  return ddsrt_memdup (&msg, sizeof (TestIdl_MsgAppendStruct1));
}

static void * sample_init_appendstruct2 (void)
{
  TestIdl_MsgAppendStruct2 msg = { .msg_field1 = 101, .msg_field2 = { .submsg_field1 = 1011, .submsg_field2 = 1022 }, .msg_field3 = 103 };
  for (uint32_t n = 0; n < 10000; n++)
  {
    msg.msg_field2.submsg_field3[n] = 1 + n;
    msg.msg_field4[n] = 1 + n;
  }
  return ddsrt_memdup (&msg, sizeof (TestIdl_MsgAppendStruct2));
}

static bool sample_equal_appendstruct1 (void *s_wr, void *s_rd)
{
  TestIdl_MsgAppendStruct1 *msg_wr = s_wr;
  TestIdl_MsgAppendStruct2 *msg_rd = s_rd;
  bool eq = true;
  for (int n = 0; n < 10000 && eq; n++)
    eq = msg_rd->msg_field2.submsg_field3[n] == 0 && msg_rd->msg_field4[n] == 0;
  return eq
    && msg_wr->msg_field1 == msg_rd->msg_field1
    && msg_wr->msg_field2.submsg_field1 == msg_rd->msg_field2.submsg_field1
    && msg_wr->msg_field2.submsg_field2 == msg_rd->msg_field2.submsg_field2
    && msg_wr->msg_field3 == msg_rd->msg_field3
  ;
}

static bool sample_equal_appendstruct2 (void *s_wr, void *s_rd)
{
  TestIdl_MsgAppendStruct2 *msg_wr = s_wr;
  TestIdl_MsgAppendStruct1 *msg_rd = s_rd;
  return
    msg_wr->msg_field1 == msg_rd->msg_field1
    && msg_wr->msg_field2.submsg_field1 == msg_rd->msg_field2.submsg_field1
    && msg_wr->msg_field2.submsg_field2 == msg_rd->msg_field2.submsg_field2
    && msg_wr->msg_field3 == msg_rd->msg_field3
  ;
}

static void sample_free_appendstruct (void *s1, void *s2)
{
  ddsrt_free (s1);
  ddsrt_free (s2);
}

/**********************************************
 * Appendable types: default values for types
 **********************************************/

/* @appendable */
typedef struct TestIdl_MsgAppendDefaults1
{
  uint32_t msg_field1;
} TestIdl_MsgAppendDefaults1;

static const uint32_t TestIdl_MsgAppendDefaults1_ops [] =
{
  DDS_OP_DLC,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_MsgAppendDefaults1, msg_field1),
  DDS_OP_RTS,
};

typedef struct TestIdl_SubMsgAppendDefaults2
{
  uint32_t submsg_field1;
} TestIdl_SubMsgAppendDefaults2;

typedef struct TestIdl_MsgAppendDefaults2_msg_field_su8_seq
{
  uint32_t _maximum;
  uint32_t _length;
  uint8_t *_buffer;
  bool _release;
} TestIdl_MsgAppendDefaults2_msg_field_su8_seq;

typedef struct TestIdl_MsgAppendDefaults2_msg_field_subm_seq
{
  uint32_t _maximum;
  uint32_t _length;
  TestIdl_SubMsgAppendDefaults2 *_buffer;
  bool _release;
} TestIdl_MsgAppendDefaults2_msg_field_subm_seq;

typedef enum TestIdl_MsgAppendDefaults2_Enum
{
  TestIdl_APPEND_DEFAULTS_KIND1,
  TestIdl_APPEND_DEFAULTS_KIND2,
  TestIdl_APPEND_DEFAULTS_KIND3
} TestIdl_MsgAppendDefaults2_Enum;

typedef struct TestIdl_MsgAppendDefaults2Union
{
  int32_t _d;
  union
  {
    int32_t field1;
    uint32_t field2;
    uint8_t field3;
  } _u;
} TestIdl_MsgAppendDefaults2Union;

/* @appendable */
typedef struct TestIdl_MsgAppendDefaults2
{
  uint32_t msg_field1;

  int8_t msg_field_i8;
  uint8_t msg_field_u8;
  int16_t msg_field_i16;
  uint16_t msg_field_u16;
  int32_t msg_field_i32;
  uint32_t msg_field_u32;
  int64_t msg_field_i64;
  uint64_t msg_field_u64;
  uint8_t msg_field_au8[100];
  uint64_t msg_field_au64[100];
  TestIdl_MsgAppendDefaults2_Enum msg_field_enum;
  char *msg_field_str;
  char msg_field_bstr[100];
  TestIdl_MsgAppendDefaults2Union msg_field_uni;
  TestIdl_MsgAppendDefaults2_msg_field_su8_seq msg_field_su8;
  TestIdl_SubMsgAppendDefaults2 msg_field_subm;
  TestIdl_SubMsgAppendDefaults2 msg_field_asubm[100];
  TestIdl_MsgAppendDefaults2_msg_field_subm_seq msg_field_ssubm;
} TestIdl_MsgAppendDefaults2;

static const uint32_t TestIdl_MsgAppendDefaults2_ops [] =
{
  DDS_OP_DLC,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_MsgAppendDefaults2, msg_field1),
  DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (TestIdl_MsgAppendDefaults2, msg_field_i8),
  DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (TestIdl_MsgAppendDefaults2, msg_field_u8),
  DDS_OP_ADR | DDS_OP_TYPE_2BY, offsetof (TestIdl_MsgAppendDefaults2, msg_field_i16),
  DDS_OP_ADR | DDS_OP_TYPE_2BY, offsetof (TestIdl_MsgAppendDefaults2, msg_field_u16),
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_MsgAppendDefaults2, msg_field_i32),
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_MsgAppendDefaults2, msg_field_u32),
  DDS_OP_ADR | DDS_OP_TYPE_8BY, offsetof (TestIdl_MsgAppendDefaults2, msg_field_i64),
  DDS_OP_ADR | DDS_OP_TYPE_8BY, offsetof (TestIdl_MsgAppendDefaults2, msg_field_u64),
  DDS_OP_ADR | DDS_OP_TYPE_ENU, offsetof (TestIdl_MsgAppendDefaults2, msg_field_enum), 2u,
  DDS_OP_ADR | DDS_OP_TYPE_STR, offsetof (TestIdl_MsgAppendDefaults2, msg_field_str),
  DDS_OP_ADR | DDS_OP_TYPE_BST, offsetof (TestIdl_MsgAppendDefaults2, msg_field_bstr), 100u,
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_1BY, offsetof (TestIdl_MsgAppendDefaults2, msg_field_au8), 100u,
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_8BY, offsetof (TestIdl_MsgAppendDefaults2, msg_field_au64), 100u,
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_1BY, offsetof (TestIdl_MsgAppendDefaults2, msg_field_su8),
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_MsgAppendDefaults2, msg_field_subm), (3u << 16u) + 27u,
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_STU, offsetof (TestIdl_MsgAppendDefaults2, msg_field_asubm), 100, (5u << 16u) + 24u, sizeof (TestIdl_SubMsgAppendDefaults2),
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU, offsetof (TestIdl_MsgAppendDefaults2, msg_field_ssubm), sizeof (TestIdl_SubMsgAppendDefaults2), (4u << 16u) + 19u,
  DDS_OP_ADR | DDS_OP_TYPE_UNI | DDS_OP_SUBTYPE_ENU, offsetof (TestIdl_MsgAppendDefaults2, msg_field_uni._d), 3u, (14u << 16) + 5u, 2u,
    DDS_OP_JEQ | DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN | 0, TestIdl_APPEND_DEFAULTS_KIND1, offsetof (TestIdl_MsgAppendDefaults2, msg_field_uni._u.field1),
    DDS_OP_JEQ | DDS_OP_TYPE_4BY | 0, TestIdl_APPEND_DEFAULTS_KIND2, offsetof (TestIdl_MsgAppendDefaults2, msg_field_uni._u.field2),
    DDS_OP_JEQ | DDS_OP_TYPE_1BY | 0, TestIdl_APPEND_DEFAULTS_KIND3, offsetof (TestIdl_MsgAppendDefaults2, msg_field_uni._u.field3),
  DDS_OP_RTS,

  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsgAppendDefaults2, submsg_field1),
  DDS_OP_RTS,
};

const dds_topic_descriptor_t TestIdl_MsgAppendDefaults1_desc = { sizeof (TestIdl_MsgAppendDefaults1), 4u, 0u, 0u, "TestIdl::MsgAppendDefaults1", NULL, 0, TestIdl_MsgAppendDefaults1_ops, "" };
const dds_topic_descriptor_t TestIdl_MsgAppendDefaults2_desc = { sizeof (TestIdl_MsgAppendDefaults2), 4u, 0u, 0u, "TestIdl::MsgAppendDefaults2", NULL, 0, TestIdl_MsgAppendDefaults2_ops, "" };

static void * sample_init_appenddefaults1 (void)
{
  TestIdl_MsgAppendDefaults1 msg = { .msg_field1 = 123 };
  return ddsrt_memdup (&msg, sizeof (TestIdl_MsgAppendDefaults1));
}

static void * sample_init_appenddefaults2 (void)
{
  TestIdl_MsgAppendDefaults2 msg;
  memset (&msg, 0xff, sizeof (msg));
  msg.msg_field_str = NULL;
  msg.msg_field_su8._length = 0;
  msg.msg_field_ssubm._length = 0;
  msg.msg_field_uni._d = 0;
  msg.msg_field_enum = TestIdl_APPEND_DEFAULTS_KIND2;
  msg.msg_field1 = 456;
  return ddsrt_memdup (&msg, sizeof (TestIdl_MsgAppendDefaults2));
}

static bool sample_equal_appenddefaults1 (void *s_wr, void *s_rd)
{
  TestIdl_MsgAppendDefaults1 *msg_wr = s_wr;
  TestIdl_MsgAppendDefaults2 *msg_rd = s_rd;
  return msg_wr->msg_field1 == msg_rd->msg_field1
    && msg_rd->msg_field_i8 == 0 && msg_rd->msg_field_u8 == 0
    && msg_rd->msg_field_i16 == 0 && msg_rd->msg_field_u16 == 0
    && msg_rd->msg_field_i32 == 0 && msg_rd->msg_field_u32 == 0
    && msg_rd->msg_field_i64 == 0 && msg_rd->msg_field_u64 == 0
    && msg_rd->msg_field_au8[0] == 0 && msg_rd->msg_field_au8[99] == 0
    && msg_rd->msg_field_au64[0] == 0 && msg_rd->msg_field_au64[99] == 0
    && msg_rd->msg_field_enum == TestIdl_APPEND_DEFAULTS_KIND1
    && msg_rd->msg_field_str != NULL && msg_rd->msg_field_str[0] == '\0'
    && msg_rd->msg_field_bstr[0] == '\0'
    && msg_rd->msg_field_uni._d == 0 && msg_rd->msg_field_uni._u.field1 == 0
    && msg_rd->msg_field_su8._length == 0
    && msg_rd->msg_field_subm.submsg_field1 == 0
    && msg_rd->msg_field_asubm[0].submsg_field1 == 0 && msg_rd->msg_field_asubm[99].submsg_field1 == 0
    && msg_rd->msg_field_ssubm._length == 0
  ;
}

static bool sample_equal_appenddefaults2 (void *s_wr, void *s_rd)
{
  TestIdl_MsgAppendDefaults2 *msg_wr = s_wr;
  TestIdl_MsgAppendDefaults1 *msg_rd = s_rd;
  return msg_wr->msg_field1 == msg_rd->msg_field1;
}

static void sample_free_appenddefaults (TestIdl_MsgAppendDefaults1 *s1, TestIdl_MsgAppendDefaults2 *s2)
{
  ddsrt_free (s1);
  ddsrt_free (s2->msg_field_str);
  ddsrt_free (s2);
}

static void sample_free_appenddefaults1 (void *s_wr, void *s_rd)
{
  sample_free_appenddefaults (s_wr, s_rd);
}

static void sample_free_appenddefaults2 (void *s_wr, void *s_rd)
{
  sample_free_appenddefaults (s_rd, s_wr);
}

/**********************************************
 * Mutable type
 **********************************************/
typedef struct TestIdl_SubMsgMutable1 {
  uint32_t submsg_field1;
  uint32_t submsg_field2[3];
} TestIdl_SubMsgMutable1;

typedef struct TestIdl_SubMsgMutable1_seq
{
  uint32_t _maximum;
  uint32_t _length;
  TestIdl_SubMsgMutable1 *_buffer;
  bool _release;
} TestIdl_SubMsgMutable1_seq;

typedef struct TestIdl_MsgMutable1 {
  uint32_t msg_field1;
  uint16_t msg_field2;
  TestIdl_SubMsgMutable1 msg_field3;
  TestIdl_SubMsgMutable1 msg_field4[2];
  int32_t msg_field5;
  double msg_field7;
  TestIdl_SubMsgMutable1 msg_field8;
  TestIdl_SubMsgMutable1_seq msg_field10;
  uint8_t msg_field11;
} TestIdl_MsgMutable1;

static const uint32_t TestIdl_MsgMutable1_ops [] =
{
  DDS_OP_PLC,
    DDS_OP_PLM | 19u, 1,
    DDS_OP_PLM | 20u, 2,
    DDS_OP_PLM | 21u, 3,
    DDS_OP_PLM | 23u, 4,
    DDS_OP_PLM | 27u, 5,
    DDS_OP_PLM | 28u, 7,
    DDS_OP_PLM | 29u, 8,
    DDS_OP_PLM | 31u, 10,
    DDS_OP_PLM | 34u, 11,
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_MU, offsetof (TestIdl_MsgMutable1, msg_field1),
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_2BY, offsetof (TestIdl_MsgMutable1, msg_field2),
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_MsgMutable1, msg_field3), (3u << 16u) + 28u,
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_STU, offsetof (TestIdl_MsgMutable1, msg_field4), 2u, (5u << 16u) + 24u, sizeof (TestIdl_SubMsgMutable1),
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN, offsetof (TestIdl_MsgMutable1, msg_field5),
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_8BY | DDS_OP_FLAG_FP, offsetof (TestIdl_MsgMutable1, msg_field7),
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_MsgMutable1, msg_field8), (3u << 16u) + 12u,
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU, offsetof (TestIdl_MsgMutable1, msg_field10), sizeof (TestIdl_SubMsgMutable1), (4u << 16u) + 8u,
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (TestIdl_MsgMutable1, msg_field11),
  DDS_OP_RTS,

  DDS_OP_PLC,
    DDS_OP_PLM | 5u, 1,
    DDS_OP_PLM | 6u, 2,
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsgMutable1, submsg_field1),
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_4BY, offsetof (TestIdl_SubMsgMutable1, submsg_field2), 3u,
  DDS_OP_RTS,
};

typedef struct TestIdl_SubMsgMutable2 {
  uint32_t submsg_field1;
  uint32_t submsg_field2[3];
} TestIdl_SubMsgMutable2;

typedef struct TestIdl_SubMsgMutable2_seq
{
  uint32_t _maximum;
  uint32_t _length;
  TestIdl_SubMsgMutable2 *_buffer;
  bool _release;
} TestIdl_SubMsgMutable2_seq;

typedef struct TestIdl_MsgMutable2 {
  uint32_t msg_field1;
  uint16_t msg_field2;
  TestIdl_SubMsgMutable2 msg_field3;
  TestIdl_SubMsgMutable2 msg_field4[2];
  int32_t msg_field6;
  double msg_field7;
  TestIdl_SubMsgMutable2 msg_field9;
  TestIdl_SubMsgMutable2_seq msg_field10;
  uint8_t msg_field11;
  TestIdl_SubMsgMutable2_seq msg_field12;
} TestIdl_MsgMutable2;

static const uint32_t TestIdl_MsgMutable2_ops [] =
{
  DDS_OP_PLC,
    DDS_OP_PLM | DDS_OP_FLAG_MU << 16 | 21u, 1,
    DDS_OP_PLM | 22u, 2,
    DDS_OP_PLM | 23u, 3,
    DDS_OP_PLM | 25u, 4,
    DDS_OP_PLM | 29u, 6,
    DDS_OP_PLM | 30u, 7,
    DDS_OP_PLM | 31u, 9,
    DDS_OP_PLM | 33u, 10,
    DDS_OP_PLM | 36u, 11,
    DDS_OP_PLM | 37u, 12,
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_MsgMutable2, msg_field1),
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_2BY, offsetof (TestIdl_MsgMutable2, msg_field2),
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_MsgMutable2, msg_field3), (3u << 16u) + 33u,
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_STU, offsetof (TestIdl_MsgMutable2, msg_field4), 2u, (5u << 16u) + 29u, sizeof (TestIdl_SubMsgMutable2),
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_SGN, offsetof (TestIdl_MsgMutable2, msg_field6),
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_8BY | DDS_OP_FLAG_FP, offsetof (TestIdl_MsgMutable2, msg_field7),
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_EXT, offsetof (TestIdl_MsgMutable2, msg_field9), (3u << 16u) + 17u,
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU, offsetof (TestIdl_MsgMutable2, msg_field10), sizeof (TestIdl_SubMsgMutable2), (4u << 16u) + 13u,
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (TestIdl_MsgMutable2, msg_field11),
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU, offsetof (TestIdl_MsgMutable2, msg_field12), sizeof (TestIdl_SubMsgMutable2), (4u << 16u) + 5u,
  DDS_OP_RTS,

  DDS_OP_PLC,
    DDS_OP_PLM | 5u, 1,
    DDS_OP_PLM | 6u, 2,
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_4BY, offsetof (TestIdl_SubMsgMutable2, submsg_field1),
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_4BY, offsetof (TestIdl_SubMsgMutable2, submsg_field2), 3u,
  DDS_OP_RTS,
};

const dds_topic_descriptor_t TestIdl_MsgMutable1_desc = { sizeof (TestIdl_MsgMutable1), 4u, 0u, 0u, "TestIdl::MsgMutable1", NULL, 0, TestIdl_MsgMutable1_ops, "" };
const dds_topic_descriptor_t TestIdl_MsgMutable2_desc = { sizeof (TestIdl_MsgMutable2), 4u, 0u, 0u, "TestIdl::MsgMutable2", NULL, 0, TestIdl_MsgMutable2_ops, "" };

static void * sample_init_mutable1 (void)
{
  TestIdl_SubMsgMutable1 sseq[] = { { .submsg_field1 = 1001, .submsg_field2 = { 1002, 1003, 1004 } }, { .submsg_field1 = 1003, .submsg_field2 = { 1005, 1006, 1007 } } };
  TestIdl_SubMsgMutable1_seq seq = { ._length = 2, ._maximum = 2, ._buffer = ddsrt_memdup (sseq, 2 * sizeof (TestIdl_SubMsgMutable1)) };
  TestIdl_MsgMutable1 msg = {
      .msg_field1 = 1,
      .msg_field2 = 2,
      .msg_field3 = { .submsg_field1 = 3, .submsg_field2 = { 4, 5, 6 } },
      .msg_field4 = { { .submsg_field1 = 10, .submsg_field2 = { 11, 12, 13 } }, { .submsg_field1 = 14, .submsg_field2 = { 15, 16, 17 } } },
      .msg_field5 = -5,
      .msg_field7 = 4.1,
      .msg_field8 = { .submsg_field1 = 8, .submsg_field2 = { 9, 10, 11 } },
      .msg_field10 = seq,
      .msg_field11 = 20
  };
  return ddsrt_memdup (&msg, sizeof (TestIdl_MsgMutable1));
}

static void * sample_init_mutable2 (void)
{
  TestIdl_SubMsgMutable2 sseq[] = { { .submsg_field1 = 2001, .submsg_field2 = { 2002, 2003, 2004 } }, { .submsg_field1 = 2003, .submsg_field2 = { 2005, 2006, 2007 } } };
  TestIdl_SubMsgMutable2_seq seq = { ._length = 2, ._maximum = 2, ._buffer = ddsrt_memdup (sseq, 2 * sizeof (TestIdl_SubMsgMutable2)) };
  TestIdl_SubMsgMutable2 sseq2[] = { { .submsg_field1 = 2001, .submsg_field2 = { 2002, 2003, 2004 } }, { .submsg_field1 = 2005, .submsg_field2 = { 2006, 2007, 2008 } }, { .submsg_field1 = 2009, .submsg_field2 = { 2010, 2011, 2012 } } };
  TestIdl_SubMsgMutable2_seq seq2 = { ._length = 3, ._maximum = 3, ._buffer = ddsrt_memdup (sseq2, 3 * sizeof (TestIdl_SubMsgMutable2)) };
  TestIdl_MsgMutable2 msg = {
      .msg_field1 = 101,
      .msg_field2 = 102,
      .msg_field3 = { .submsg_field1 = 103, .submsg_field2 = { 104, 105, 106 } },
      .msg_field4 = { { .submsg_field1 = 1010, .submsg_field2 = { 1011, 1012, 1013 } }, { .submsg_field1 = 1014, .submsg_field2 = { 1015, 1016, 1017 } } },
      .msg_field6 = -106,
      .msg_field7 = 104.1,
      .msg_field9 = { .submsg_field1 = 109, .submsg_field2 = { 1010, 1011, 1012 } },
      .msg_field10 = seq,
      .msg_field11 = 254,
      .msg_field12 = seq2,
  };
  return ddsrt_memdup (&msg, sizeof (TestIdl_MsgMutable2));
}

static bool sample_equal_mutable1_intseq (uint32_t *seq1, uint32_t *seq2, uint32_t len)
{
  for (uint32_t n = 0; n < len; n++)
    if (seq1[n] != seq2[n])
      return false;
  return true;
}

static bool sample_equal_mutable_TestIdl_SubMsgMutable_seq (TestIdl_SubMsgMutable1_seq *s1, TestIdl_SubMsgMutable2_seq *s2)
{
  if (s1->_length != s2->_length)
    return false;
  for (uint32_t n = 0; n < s1->_length; n++)
  {
    if (s1->_buffer[n].submsg_field1 != s2->_buffer[n].submsg_field1
      || !sample_equal_mutable1_intseq (s1->_buffer[n].submsg_field2, s2->_buffer[n].submsg_field2, 3))
      return false;
  }
  return true;
}

static bool sample_equal_mutable1 (void *s_wr, void *s_rd)
{
  TestIdl_MsgMutable1 *msg_wr = s_wr;
  TestIdl_MsgMutable2 *msg_rd = s_rd;
  return msg_wr->msg_field1 == msg_rd->msg_field1
    && msg_wr->msg_field2 == msg_rd->msg_field2
    && msg_wr->msg_field3.submsg_field1 == msg_rd->msg_field3.submsg_field1 && sample_equal_mutable1_intseq (msg_wr->msg_field3.submsg_field2, msg_rd->msg_field3.submsg_field2, 3)
    && msg_wr->msg_field4[0].submsg_field1 == msg_rd->msg_field4[0].submsg_field1 && sample_equal_mutable1_intseq (msg_wr->msg_field4[0].submsg_field2, msg_rd->msg_field4[0].submsg_field2, 3)
      && msg_wr->msg_field4[1].submsg_field1 == msg_rd->msg_field4[1].submsg_field1 && sample_equal_mutable1_intseq (msg_wr->msg_field4[1].submsg_field2, msg_rd->msg_field4[1].submsg_field2, 3)
    && msg_rd->msg_field6 == 0
    && msg_wr->msg_field7 == msg_rd->msg_field7
    && msg_rd->msg_field9.submsg_field1 == 0 && msg_rd->msg_field9.submsg_field2[0] == 0 && msg_rd->msg_field9.submsg_field2[1] == 0 && msg_rd->msg_field9.submsg_field2[2] == 0
    && msg_wr->msg_field10._length == msg_rd->msg_field10._length
    && sample_equal_mutable_TestIdl_SubMsgMutable_seq (&msg_wr->msg_field10, &msg_rd->msg_field10)
    && msg_wr->msg_field11 == msg_rd->msg_field11
    && msg_rd->msg_field12._length == 0
  ;
}

static bool sample_equal_mutable2 (void *s_wr, void *s_rd)
{
  TestIdl_MsgMutable2 *msg_wr = s_wr;
  TestIdl_MsgMutable1 *msg_rd = s_rd;
  return msg_wr->msg_field1 == msg_rd->msg_field1
    && msg_wr->msg_field2 == msg_rd->msg_field2
    && msg_wr->msg_field3.submsg_field1 == msg_rd->msg_field3.submsg_field1 && sample_equal_mutable1_intseq (msg_wr->msg_field3.submsg_field2, msg_rd->msg_field3.submsg_field2, 3)
    && msg_wr->msg_field4[0].submsg_field1 == msg_rd->msg_field4[0].submsg_field1 && sample_equal_mutable1_intseq (msg_wr->msg_field4[0].submsg_field2, msg_rd->msg_field4[0].submsg_field2, 3)
      && msg_wr->msg_field4[1].submsg_field1 == msg_rd->msg_field4[1].submsg_field1 && sample_equal_mutable1_intseq (msg_wr->msg_field4[1].submsg_field2, msg_rd->msg_field4[1].submsg_field2, 3)
    && msg_rd->msg_field5 == 0
    && msg_wr->msg_field7 == msg_rd->msg_field7
    && msg_rd->msg_field8.submsg_field1 == 0 && msg_rd->msg_field8.submsg_field2[0] == 0 && msg_rd->msg_field8.submsg_field2[1] == 0 && msg_rd->msg_field8.submsg_field2[2] == 0
    && msg_wr->msg_field10._length == msg_rd->msg_field10._length
    && sample_equal_mutable_TestIdl_SubMsgMutable_seq (&msg_rd->msg_field10, &msg_wr->msg_field10)
    && msg_wr->msg_field11 == msg_rd->msg_field11
  ;
}

static void sample_free_mutable (TestIdl_MsgMutable1 *s1, TestIdl_MsgMutable2 *s2)
{
  ddsrt_free (s1->msg_field10._buffer);
  ddsrt_free (s2->msg_field10._buffer);
  ddsrt_free (s2->msg_field12._buffer);
  ddsrt_free (s1);
  ddsrt_free (s2);
}

static void sample_free_mutable1 (void *s_wr, void *s_rd)
{
  sample_free_mutable (s_wr, s_rd);
}

static void sample_free_mutable2 (void *s_wr, void *s_rd)
{
  sample_free_mutable (s_rd, s_wr);
}


/**********************************************
 * Generic implementation and tests
 **********************************************/

static dds_entity_t d1, d2, tp1, tp2, dp1, dp2, rd, wr;

static void cdrstream_init (void)
{
  char * conf = ddsrt_expand_envvars (DDS_CONFIG, DDS_DOMAINID1);
  d1 = dds_create_domain (DDS_DOMAINID1, conf);
  CU_ASSERT_FATAL (d1 > 0);
  ddsrt_free (conf);
  conf = ddsrt_expand_envvars (DDS_CONFIG, DDS_DOMAINID2);
  d2 = dds_create_domain (DDS_DOMAINID2, conf);
  CU_ASSERT_FATAL (d2 > 0);
  ddsrt_free (conf);

  dp1 = dds_create_participant (DDS_DOMAINID1, NULL, NULL);
  CU_ASSERT_FATAL (dp1 > 0);
  dp2 = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  CU_ASSERT_FATAL (dp2 > 0);
}

static void entity_init (const dds_topic_descriptor_t *desc, dds_data_representation_id_t data_representation, bool exp_rd_wr_fail)
{
  char topicname[100];
  create_unique_topic_name ("ddsc_cdrstream", topicname, sizeof topicname);

  tp1 = dds_create_topic (dp1, desc, topicname, NULL, NULL);
  CU_ASSERT_FATAL (tp1 > 0);
  tp2 = dds_create_topic (dp2, desc, topicname, NULL, NULL);
  CU_ASSERT_FATAL (tp2 > 0);

  dds_qos_t *qos = dds_create_qos ();
  dds_qset_history(qos, DDS_HISTORY_KEEP_ALL, DDS_LENGTH_UNLIMITED);
  dds_qset_durability(qos, DDS_DURABILITY_TRANSIENT_LOCAL);
  dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  dds_qset_data_representation (qos, 1, (dds_data_representation_id_t[]) { data_representation });

  rd = dds_create_reader (dp2, tp2, qos, NULL);
  CU_ASSERT_FATAL ((!exp_rd_wr_fail && rd > 0) || (exp_rd_wr_fail && rd <= 0));
  wr = dds_create_writer (dp1, tp1, qos, NULL);
  CU_ASSERT_FATAL ((!exp_rd_wr_fail && wr > 0) || (exp_rd_wr_fail && wr <= 0));
  if (!exp_rd_wr_fail)
    sync_reader_writer (dp2, rd, dp1, wr);
  dds_delete_qos (qos);
}

static void cdrstream_fini (void)
{
  dds_delete (d1);
  dds_delete (d2);
}

#define D(n) TestIdl_Msg ## n ## _desc
#define E(n) sample_empty_ ## n
#define I(n) sample_init_ ## n
#define K(n) keys_equal_ ## n
#define C(n) sample_equal_ ## n
#define F(n) sample_free_ ## n

CU_TheoryDataPoints (ddsc_cdrstream, ser_des) = {
  CU_DataPoints (const char *,                   "nested structs",
  /*                                             |          */"string types",
  /*                                             |           |       */"unions",
  /*                                             |           |        |         */"recursive",
  /*                                             |           |        |          |             */"appendable",
  /*                                             |           |        |          |              |              */"keys nested",
  /*                                             |           |        |          |              |               |              */"arrays" ),
  CU_DataPoints (const dds_topic_descriptor_t *, &D(Nested), &D(Str), &D(Union), &D(Recursive), &D(Appendable), &D(KeysNested), &D(Arr)  ),
  CU_DataPoints (sample_empty,                   0,           0,       0,         0,             0,              E(keysnested),  0       ),
  CU_DataPoints (sample_init,                    I(nested),   I(str),  I(union),  I(recursive),  I(appendable),  I(keysnested),  I(arr)  ),
  CU_DataPoints (keys_equal,                     0,           0,       0,         0,             0,              K(keysnested),  0       ),
  CU_DataPoints (sample_equal,                   C(nested),   C(str),  C(union),  C(recursive),  C(appendable),  C(keysnested),  C(arr)  ),
  CU_DataPoints (sample_free,                    F(nested),   F(str),  F(union),  F(recursive),  F(appendable),  F(keysnested),  F(arr)  ),
};

CU_Theory ((const char *descr, const dds_topic_descriptor_t *desc, sample_empty sample_empty_fn, sample_init sample_init_fn, keys_equal keys_equal_fn, sample_equal sample_equal_fn, sample_free sample_free_fn),
    ddsc_cdrstream, ser_des, .init = cdrstream_init, .fini = cdrstream_fini)
{
  dds_return_t ret;
  tprintf ("Running test ser_des: %s\n", descr);

  entity_init (desc, DDS_DATA_REPRESENTATION_XCDR2, false);
  dds_set_status_mask (rd, DDS_DATA_AVAILABLE_STATUS);
  dds_entity_t ws = dds_create_waitset (dp2);
  ret = dds_waitset_attach (ws, rd, rd);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

  void * msg = sample_init_fn ();

  ret = dds_write (wr, msg);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  if (desc->m_nkeys > 0)
  {
    assert (sample_empty_fn);
    assert (keys_equal_fn);
    void * key_data = sample_empty_fn ();
    dds_instance_handle_t ih = dds_lookup_instance (wr, msg);
    CU_ASSERT_PTR_NOT_NULL_FATAL (ih);
    ret = dds_instance_get_key(wr, ih, key_data);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
    bool eq = keys_equal_fn (msg, key_data);
    CU_ASSERT_FATAL (eq);
    sample_free_fn (key_data);
  }

  dds_attach_t triggered;
  ret = dds_waitset_wait (ws, &triggered, 1, DDS_SECS(5));
  CU_ASSERT_EQUAL_FATAL (ret, 1);

  void * rds[1] = { NULL };
  dds_sample_info_t si[1];
  ret = dds_read (rd, rds, si, 1, 1);
  CU_ASSERT_EQUAL_FATAL (ret, 1);
  bool eq = sample_equal_fn (msg, rds[0]);
  CU_ASSERT_FATAL (eq);
  dds_return_loan (rd, rds, 1);

  /* In case type has keys, write a dispose so that write key
     and read key code from cdrstream serializer is used */
  if (desc->m_nkeys > 0)
  {
    ret = dds_dispose (wr, msg);
    CU_ASSERT_EQUAL_FATAL (ret, 0);
    ret = dds_waitset_wait (ws, &triggered, 1, DDS_SECS(5));
    CU_ASSERT_EQUAL_FATAL (ret, 1);
    ret = dds_read (rd, rds, si, 1, 1);
    CU_ASSERT_EQUAL_FATAL (ret, 1);
    CU_ASSERT_EQUAL_FATAL (si->instance_state, DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE);
    dds_return_loan (rd, rds, 1);
  }

  // cleanup
  sample_free_fn (msg);
}

CU_TheoryDataPoints (ddsc_cdrstream, ser_des_multiple) = {
  CU_DataPoints (const char *,                   "nested structs",
  /*                                             |          */"string types",
  /*                                             |           |       */"unions",
  /*                                             |           |        |         */"recursive",
  /*                                             |           |        |          |             */"appendable",
  /*                                             |           |        |          |              |              */"keys nested",
  /*                                             |           |        |          |              |               |              */"arrays",
  /*                                             |           |        |          |              |               |               |       */"ext",
  /*                                             |           |        |          |              |               |               |        |       */"opt"  ),
  CU_DataPoints (const dds_topic_descriptor_t *, &D(Nested), &D(Str), &D(Union), &D(Recursive), &D(Appendable), &D(KeysNested), &D(Arr), &D(Ext), &D(Opt) ),
  CU_DataPoints (sample_init,                    I(nested),   I(str),  I(union),  I(recursive),  I(appendable),  I(keysnested),  I(arr),  I(ext), I(opt)  ),
  CU_DataPoints (sample_equal,                   C(nested),   C(str),  C(union),  C(recursive),  C(appendable),  C(keysnested),  C(arr),  C(ext), C(opt)  ),
  CU_DataPoints (sample_free,                    F(nested),   F(str),  F(union),  F(recursive),  F(appendable),  F(keysnested),  F(arr),  F(ext), F(opt)  ),
};

#define NUM_SAMPLES 10
CU_Theory ((const char *descr, const dds_topic_descriptor_t *desc, sample_init sample_init_fn, sample_equal sample_equal_fn, sample_free sample_free_fn),
    ddsc_cdrstream, ser_des_multiple, .init = cdrstream_init, .fini = cdrstream_fini)
{
  dds_return_t ret;
  tprintf ("Running test ser_des_multiple: %s\n", descr);

  entity_init (desc, DDS_DATA_REPRESENTATION_XCDR2, false);

  void * rds[1] = { NULL };
  for (int n = 0; n < NUM_SAMPLES; n++)
  {
    void * msg = sample_init_fn ();
    ret = dds_write (wr, msg);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
    while (ret <= 0)
    {
      dds_sample_info_t si[1];
      if ((ret = dds_take (rd, rds, si, 1, 1)) > 0)
      {
        CU_ASSERT_EQUAL_FATAL (ret, 1);
        bool eq = sample_equal_fn (msg, rds[0]);
        CU_ASSERT_FATAL (eq);
      }
      else
        dds_sleepfor (DDS_MSECS (10));
    }
    sample_free_fn (msg);
  }
  dds_return_loan (rd, rds, 1);
}
#undef NUM_SAMPLES

CU_TheoryDataPoints (ddsc_cdrstream, appendable_mutable) = {
  CU_DataPoints (const char *,                   "appendable struct",
  /*                                              |                 */"appendable defaults",
  /*                                              |                  |                     */"mutable"),
  CU_DataPoints (const dds_topic_descriptor_t *, &D(AppendStruct1), &D(AppendDefaults1),  &D(Mutable1) ),
  CU_DataPoints (const dds_topic_descriptor_t *, &D(AppendStruct2), &D(AppendDefaults2),  &D(Mutable2) ),
  CU_DataPoints (sample_init,                     I(appendstruct1),  I(appenddefaults1),   I(mutable1) ),
  CU_DataPoints (sample_init,                     I(appendstruct2),  I(appenddefaults2),   I(mutable2) ),
  CU_DataPoints (sample_equal,                    C(appendstruct1),  C(appenddefaults1),   C(mutable1) ),
  CU_DataPoints (sample_equal,                    C(appendstruct2),  C(appenddefaults2),   C(mutable2) ),
  CU_DataPoints (sample_free2,                    F(appendstruct),   F(appenddefaults1),   F(mutable1) ),
  CU_DataPoints (sample_free2,                    F(appendstruct),   F(appenddefaults2),   F(mutable2) ),
};

CU_Theory ((const char *descr, const dds_topic_descriptor_t *desc1, const dds_topic_descriptor_t *desc2,
    sample_init sample_init_fn1, sample_init sample_init_fn2, sample_equal sample_equal_fn1, sample_equal sample_equal_fn2, sample_free2 sample_free_fn1, sample_free2 sample_free_fn2),
    ddsc_cdrstream, appendable_mutable, .init = cdrstream_init, .fini = cdrstream_fini)
{
  for (int t = 0; t <= 1; t++)
  {
    tprintf ("Running test appendable_mutable: %s (run %d/2)\n", descr, t + 1);

    const dds_topic_descriptor_t *topic_desc_wr = t ? desc2 : desc1;
    const dds_topic_descriptor_t *topic_desc_rd = t ? desc1 : desc2;

    /* Write data */
    dds_ostream_t os;
    os.m_buffer = NULL;
    os.m_index = 0;
    os.m_size = 0;
    os.m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2;

    struct dds_cdrstream_desc desc_wr;
    memset (&desc_wr, 0, sizeof (desc_wr));
    desc_wr = (struct dds_cdrstream_desc) {
      .size = topic_desc_wr->m_size,
      .align = topic_desc_wr->m_align,
      .flagset = topic_desc_wr->m_flagset,
      .keys.nkeys = 0,
      .keys.keys = NULL,
      .ops.nops = dds_stream_countops (topic_desc_wr->m_ops, topic_desc_wr->m_nkeys, topic_desc_wr->m_keys),
      .ops.ops = (uint32_t *) topic_desc_wr->m_ops,
      .member_ids.op0 = (uint32_t *) topic_desc_wr->m_ops,
      .member_ids.table = NULL
    };

    void * msg_wr = t ? sample_init_fn2 () : sample_init_fn1 ();
    bool ret = dds_stream_write_sample (&os, &dds_cdrstream_default_allocator, msg_wr, &desc_wr);
    CU_ASSERT_FATAL (ret);

    /* Read data */
    dds_istream_t is;
    is.m_buffer = os.m_buffer;
    is.m_index = 0;
    is.m_size = os.m_size;
    is.m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2;

    struct dds_cdrstream_desc desc_rd;
    memset (&desc_rd, 0, sizeof (desc_rd));
    desc_rd = (struct dds_cdrstream_desc) {
      .size = topic_desc_rd->m_size,
      .align = topic_desc_rd->m_align,
      .flagset = topic_desc_rd->m_flagset,
      .keys.nkeys = 0,
      .keys.keys = NULL,
      .ops.nops = dds_stream_countops (topic_desc_rd->m_ops, topic_desc_rd->m_nkeys, topic_desc_rd->m_keys),
      .ops.ops = (uint32_t *) topic_desc_rd->m_ops,
      .member_ids.op0 = (uint32_t *) topic_desc_wr->m_ops,
      .member_ids.table = NULL
    };

    void *msg_rd = ddsrt_calloc (1, desc_rd.size);
    dds_stream_read_sample (&is, msg_rd, &dds_cdrstream_default_allocator, &desc_rd);

    /* Check for expected result */
    bool eq = t ? sample_equal_fn2 (msg_wr, msg_rd) : sample_equal_fn1 (msg_wr, msg_rd);
    CU_ASSERT_FATAL (eq);

    /* print result */
    char buf[5000];
    is.m_index = 0;
    dds_stream_print_sample (&is, &desc_rd, buf, 5000);
    printf ("read sample: %s\n\n", buf);

    // cleanup
    t ? sample_free_fn2 (msg_wr, msg_rd) : sample_free_fn1 (msg_wr, msg_rd);
    dds_free (os.m_buffer);
  }
}

#undef D
#undef E
#undef I
#undef K
#undef C
#undef F

#define D(n) (&MinXcdrVersion_ ## n ## _desc)
CU_Test (ddsc_cdrstream, min_xcdr_version)
{
  static const struct {
    const dds_topic_descriptor_t *desc;
    uint16_t min_xcdrv;
  } tests[] = {
    { D(t), XCDR1 },
    { D(t_nested), XCDR1 },
    { D(t_inherit), XCDR1 },
    { D(t_opt), XCDR1 },
    { D(t_ext), XCDR1 },
    { D(t_append), XCDR1 },
    { D(t_append_u), XCDR1 },
    { D(t_append_nested), XCDR2 },
    { D(t_append_nested_u), XCDR2 },
    { D(t_append_opt), XCDR1 },
    { D(t_append_opt_u), XCDR1 },
    { D(t_append_seq), XCDR2 },
    { D(t_append_bseq), XCDR2 },
    { D(t_append_arr), XCDR2 },
    { D(t_mut), XCDR2 },
    { D(t_nested_mut), XCDR2 },
    { D(t_nested_opt), XCDR1 }
  };

  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    printf("running test for desc: %s\n", tests[i].desc->m_typename);
    cdrstream_init ();
    CU_ASSERT_EQUAL_FATAL (dds_stream_minimum_xcdr_version (tests[i].desc->m_ops), tests[i].min_xcdrv);

    entity_init (tests[i].desc, DDS_DATA_REPRESENTATION_XCDR1, tests[i].min_xcdrv != XCDR1);
    entity_init (tests[i].desc, DDS_DATA_REPRESENTATION_XCDR2, false);
    cdrstream_fini ();
  }
}
#undef D


#define D(n) (&CdrStreamOptimize_ ## n ## _desc)
CU_Test (ddsc_cdrstream, check_optimize)
{
  static const struct {
    const dds_topic_descriptor_t *desc;
    size_t opt_size_xcdr1;
    size_t opt_size_xcdr2;
    const char *description;
  } tests[] = {
    { D(t1),     4,    4, "final type" },
    { D(t1_a),   0,    0, "appendable type: has DHEADER in CDR" },
    { D(t1_m),   0,    0, "mutable type: has EMHEADER and DHEADERS" },
    { D(t2),    16,    0, "XCDR2 uses 4 byte padding for 64-bits type, does not match memory layout" },
    { D(t3),     0,    0, "external field is pointer type" },
    { D(t4),     5,    5, "nested struct (aligned at 4 byte) at offset 0 can be optimized" },
    { D(t4a),   13,   13, "2 nested structs, alignment in CDR equal to memory alignment" },
    { D(t4b),   29,   29, "2 levels of nesting, also using same alignment in CDR and memory" },
    { D(t5),    16,    0, "XCDR2 uses 4 byte alignment for 64 bits types" },
    { D(t5a),    0,    0, "array of non-primitive type is currently not optimized (FIXME: could be optimized for XCDR1?)" },
    { D(t6),    16,   16, "CDR and memory have equal alignment" },
    { D(t6a),    0,    0, "CDR and memory have equal alignment but boolean prevents optimization" },
    { D(t7),     0,    0, "field f2 is 1-byte aligned in CDR (because of 1-byte type in nested type), but 2-byte in memory" },
    { D(t8),     0,    0, "type of f2 is appendable" },
    { D(t9),     3,    0, "bitmask (bit bound 8) array (dheader in v2)" },
    { D(t10),   12,    0, "enum (bit bound 32) array (dheader in v2)" },
    { D(t11),  410,  410, "final type with array" },
    { D(t11a),   0,    0, "final type with array but boolean prevents optimization" },
    { D(t12),    4,    4, "32 bits bitmask" },
    { D(t13),    1,    1, "8 bit bitmask" },
    { D(t14),    8,    8, "64 bits bitmask" },
    { D(t15),  100,  100, "2 levels of nesting with array in inner type" },
    { D(t16),    0,    0, "string member is ptr" },
    { D(t17),    0,    0, "string array is array of ptrs" },
    { D(t18),    0,    0, "sequence has ptr" },
    { D(t19),    0,    0, "external member is ptr" },
    { D(t20),    0,    0, "external array is ptr" },
    { D(t21),    0,    0, "8 bit enum maps to 32 bits enum in memory" },
    { D(t22),    0,    0, "16 bits enum maps to 32 bits enum in memory" },
    { D(t23),    0,    0, "external nested struct is ptr" },
    { D(t24),    0,    0, "external memner in nested struct" },
    { D(t25),    0,    0, "union type currently not optimized" },
    { D(t26),    0,    0, "union type member currently not optimized" },
    { D(t27),   16,    0, "inheritance, base members before derived type members, xcdr2 has 4 byte alignment for long long" },
    { D(t28),    0,    0, "array of booleans" },
    { D(t29),    0,    0, "boolean in extended struct" },
    { D(t30),    0,    0, "boolean in base struct" }
  };

  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    printf("running test for desc %s: %s ", tests[i].desc->m_typename, tests[i].description);
    struct dds_cdrstream_desc ddsi_desc = { .ops.nops = tests[i].desc->m_nops, .ops.ops = (uint32_t *) tests[i].desc->m_ops, .size = tests[i].desc->m_size };
    size_t opt1 = dds_stream_check_optimize (&ddsi_desc, XCDR1);
    size_t opt2 = dds_stream_check_optimize (&ddsi_desc, XCDR2);
    printf ("(opt cdr1: %zu, cdr2: %zu)\n", opt1, opt2);
    CU_ASSERT_EQUAL_FATAL (opt1, tests[i].opt_size_xcdr1);
    CU_ASSERT_EQUAL_FATAL (opt2, tests[i].opt_size_xcdr2);
  }
}
#undef D


#define D(n) (&CdrStreamDataTypeInfo_ ## n ## _desc)
CU_Test (ddsc_cdrstream, data_type_info)
{
  static const struct {
    const dds_topic_descriptor_t *desc;
    uint64_t data_types;
  } tests[] = {
    { D(dti_struct),   DDS_DATA_TYPE_CONTAINS_STRUCT | DDS_DATA_TYPE_IS_MEMCPY_SAFE },
    { D(dti_string),   DDS_DATA_TYPE_CONTAINS_STRUCT | DDS_DATA_TYPE_CONTAINS_STRING },
    { D(dti_bstring),  DDS_DATA_TYPE_CONTAINS_STRUCT | DDS_DATA_TYPE_CONTAINS_BSTRING | DDS_DATA_TYPE_IS_MEMCPY_SAFE },
    { D(dti_seq),      DDS_DATA_TYPE_CONTAINS_STRUCT | DDS_DATA_TYPE_CONTAINS_SEQUENCE },
    { D(dti_bseq),     DDS_DATA_TYPE_CONTAINS_STRUCT | DDS_DATA_TYPE_CONTAINS_BSEQUENCE },
    { D(dti_seq_str),  DDS_DATA_TYPE_CONTAINS_STRUCT | DDS_DATA_TYPE_CONTAINS_SEQUENCE | DDS_DATA_TYPE_CONTAINS_STRING },
    { D(dti_arr),      DDS_DATA_TYPE_CONTAINS_STRUCT | DDS_DATA_TYPE_CONTAINS_ARRAY | DDS_DATA_TYPE_IS_MEMCPY_SAFE },
    { D(dti_arr_bstr), DDS_DATA_TYPE_CONTAINS_STRUCT | DDS_DATA_TYPE_CONTAINS_ARRAY | DDS_DATA_TYPE_CONTAINS_BSTRING | DDS_DATA_TYPE_IS_MEMCPY_SAFE },
    { D(dti_opt),      DDS_DATA_TYPE_CONTAINS_STRUCT | DDS_DATA_TYPE_CONTAINS_OPTIONAL | DDS_DATA_TYPE_CONTAINS_EXTERNAL },
    { D(dti_ext),      DDS_DATA_TYPE_CONTAINS_STRUCT | DDS_DATA_TYPE_CONTAINS_EXTERNAL },
    { D(dti_struct_key),          DDS_DATA_TYPE_CONTAINS_STRUCT | DDS_DATA_TYPE_CONTAINS_KEY | DDS_DATA_TYPE_IS_MEMCPY_SAFE },
    { D(dti_struct_nested_key),   DDS_DATA_TYPE_CONTAINS_STRUCT | DDS_DATA_TYPE_CONTAINS_KEY | DDS_DATA_TYPE_IS_MEMCPY_SAFE },
    { D(dti_struct_nested_nokey), DDS_DATA_TYPE_CONTAINS_STRUCT | DDS_DATA_TYPE_IS_MEMCPY_SAFE },
    { D(dti_union),    DDS_DATA_TYPE_CONTAINS_UNION | DDS_DATA_TYPE_IS_MEMCPY_SAFE },
    { D(dti_union_string),  DDS_DATA_TYPE_CONTAINS_UNION | DDS_DATA_TYPE_CONTAINS_STRING },
    { D(dti_union_enum),    DDS_DATA_TYPE_CONTAINS_UNION | DDS_DATA_TYPE_CONTAINS_ENUM | DDS_DATA_TYPE_IS_MEMCPY_SAFE },
    { D(dti_union_seq),     DDS_DATA_TYPE_CONTAINS_UNION | DDS_DATA_TYPE_CONTAINS_SEQUENCE },
    { D(dti_union_arr),     DDS_DATA_TYPE_CONTAINS_UNION | DDS_DATA_TYPE_CONTAINS_ARRAY | DDS_DATA_TYPE_IS_MEMCPY_SAFE },
    { D(dti_union_struct),  DDS_DATA_TYPE_CONTAINS_UNION | DDS_DATA_TYPE_CONTAINS_STRUCT | DDS_DATA_TYPE_IS_MEMCPY_SAFE }
  };

  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    printf("running test for desc %s ", tests[i].desc->m_typename);
    uint64_t data_types = dds_stream_data_types (tests[i].desc->m_ops);
    printf ("(data types actual %"PRIu64", expected %"PRIu64")\n", data_types, tests[i].data_types);
    CU_ASSERT_EQUAL_FATAL (data_types, tests[i].data_types);
  }
}
#undef D



#undef XCDR1
#undef XCDR2


// Skip-default tests
typedef void sample_init_fn (uint8_t *data);
typedef void default_check_fn (uint8_t *data);

static void init_sub1 (uint8_t *data)
{
  struct CdrStreamSkipDefault_t1_sub *t1 = (struct CdrStreamSkipDefault_t1_sub *) data;
  t1->f2.s2 = NULL;
  t1->f2.s3 = (dds_sequence_string) { ._length = 1, ._maximum = 1, ._release = true, ._buffer = ddsrt_malloc (1 * sizeof (*t1->f2.s3._buffer)) };
  t1->f2.s3._buffer[0] = ddsrt_strdup ("test");
}

static void check_t1 (uint8_t *data)
{
  struct CdrStreamSkipDefault_t1_sub *t1 = (struct CdrStreamSkipDefault_t1_sub *) data;
  CU_ASSERT_EQUAL_FATAL (t1->f2.s1, 0);
  CU_ASSERT_EQUAL_FATAL (strlen (t1->f2.s2), 0);
  CU_ASSERT_EQUAL_FATAL (t1->f2.s3._length, 0);
  CU_ASSERT_EQUAL_FATAL (t1->f2.s3._maximum, 1);
  CU_ASSERT_EQUAL_FATAL (t1->f2.s3._release, true);
  CU_ASSERT_NOT_EQUAL_FATAL (t1->f2.s3._buffer, NULL);
}

static void init_sub2 (uint8_t *data)
{
  struct CdrStreamSkipDefault_t2_sub *t2 = (struct CdrStreamSkipDefault_t2_sub *) data;
  t2->f2.s2 = ddsrt_strdup ("test");
  t2->f2.s4 = NULL;
}

static void check_t2 (uint8_t *data)
{
  struct CdrStreamSkipDefault_t2_sub *t2 = (struct CdrStreamSkipDefault_t2_sub *) data;
  CU_ASSERT_EQUAL_FATAL (t2->f2.s1, 0);
  CU_ASSERT_EQUAL_FATAL (strlen (t2->f2.s2), 0);
  CU_ASSERT_EQUAL_FATAL (t2->f2.s3, 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL (t2->f2.s4);
  CU_ASSERT_EQUAL_FATAL (*t2->f2.s4, 0);
  CU_ASSERT_EQUAL_FATAL (t2->f3, 0.0);
}

static void init_sub3 (uint8_t *data)
{
  struct CdrStreamSkipDefault_t3_sub *t3 = (struct CdrStreamSkipDefault_t3_sub *) data;
  t3->f2.s2.s1 = NULL;
  t3->f2.s2.s2 = (dds_sequence_long) { ._length = 0, ._maximum = 0, ._release = false };
  t3->f4.s1 = NULL;
  t3->f4.s2 = (dds_sequence_long) { ._length = 2, ._maximum = 2, ._release = true, ._buffer = ddsrt_malloc (2 * sizeof (*t3->f4.s2._buffer)) };
}

static void check_t3 (uint8_t *data)
{
  struct CdrStreamSkipDefault_t3_sub *t3 = (struct CdrStreamSkipDefault_t3_sub *) data;
  CU_ASSERT_EQUAL_FATAL (t3->f2.s1, 0);
  CU_ASSERT_EQUAL_FATAL (strlen (t3->f2.s2.s1), 0);
  CU_ASSERT_EQUAL_FATAL (t3->f2.s2.s2._length, 0);
  CU_ASSERT_EQUAL_FATAL (t3->f2.s2.s2._maximum, 0);
  CU_ASSERT_EQUAL_FATAL (t3->f3, 0);
  CU_ASSERT_EQUAL_FATAL (strlen (t3->f4.s1), 0);
  CU_ASSERT_EQUAL_FATAL (t3->f4.s2._length, 0);
  CU_ASSERT_EQUAL_FATAL (t3->f4.s2._maximum, 2);
  CU_ASSERT_EQUAL_FATAL (t3->f4.s2._release, true);
  CU_ASSERT_NOT_EQUAL_FATAL (t3->f4.s2._buffer, NULL);
}

static void init_sub4 (uint8_t *data)
{
  struct CdrStreamSkipDefault_t4_sub *t4 = (struct CdrStreamSkipDefault_t4_sub *) data;
  t4->f2.s2.s2 = ddsrt_malloc (sizeof (*t4->f2.s2.s2));
  (*t4->f2.s2.s2) = (dds_sequence_long) { ._length = 3, ._maximum = 3, ._release = true, ._buffer = ddsrt_malloc (3 * sizeof (*t4->f2.s2.s2->_buffer)) };
  t4->f4.s2 = ddsrt_malloc (sizeof (*t4->f2.s2.s2));
  (*t4->f4.s2) = (dds_sequence_long) { ._length = 1, ._maximum = 4, ._release = true, ._buffer = ddsrt_malloc (4 * sizeof (*t4->f4.s2->_buffer)) };
}

static void check_t4 (uint8_t *data)
{
  struct CdrStreamSkipDefault_t4_sub *t4 = (struct CdrStreamSkipDefault_t4_sub *) data;
  CU_ASSERT_EQUAL_FATAL (t4->f2.s1, 0);
  CU_ASSERT_EQUAL_FATAL (t4->f2.s2.s1, 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL (t4->f2.s2.s2);
  CU_ASSERT_EQUAL_FATAL (t4->f2.s2.s2->_length, 0); // only length is reset, buffer and max are retained
  CU_ASSERT_EQUAL_FATAL (t4->f4.s1, 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL (t4->f4.s2);
  CU_ASSERT_EQUAL_FATAL (t4->f4.s2->_length, 0);
}


static void init_sub5 (uint8_t *data)
{
  struct CdrStreamSkipDefault_t5_sub *t5 = (struct CdrStreamSkipDefault_t5_sub *) data;
  t5->f2.s2.s2 = (dds_sequence_long) { ._release = false, ._length = 0, ._maximum = 0 };
}

static void check_t5 (uint8_t *data)
{
  struct CdrStreamSkipDefault_t5_sub *t5 = (struct CdrStreamSkipDefault_t5_sub *) data;
  CU_ASSERT_EQUAL_FATAL (t5->f2.s2.s1, 0);
  CU_ASSERT_EQUAL_FATAL (t5->f2.s2.s2._length, 0);
  CU_ASSERT_EQUAL_FATAL (t5->f2.s2.s2._maximum, 0);
}

#define D(n) (&CdrStreamSkipDefault_ ## n ## _desc)
CU_Test (ddsc_cdrstream, skip_default)
{
  static const struct {
    const dds_topic_descriptor_t *desc_pub;
    const dds_topic_descriptor_t *desc_sub;
    sample_init_fn *init_sub;
    default_check_fn *check_sub;
    const char *description;
  } tests[] = {
    { D(t1_pub), D(t1_sub), init_sub1, check_t1, "appendable top-level, appendable member" },
    { D(t2_pub), D(t2_sub), init_sub2, check_t2, "appendable top-level, mutable member" },
    { D(t3_pub), D(t3_sub), init_sub3, check_t3, "mutable top-level, nested mutable member" },
    { D(t4_pub), D(t4_sub), init_sub4, check_t4, "mutable top-level, nested appendable member" },
    { D(t5_pub), D(t5_sub), init_sub5, check_t5, "top-level equal, mutable member different" }
  };

  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    printf("running test for desc %s/%s: %s\n", tests[i].desc_pub->m_typename, tests[i].desc_sub->m_typename, tests[i].description);

    struct dds_cdrstream_desc desc_pub, desc_sub;
    dds_cdrstream_desc_from_topic_desc (&desc_pub, tests[i].desc_pub);
    assert (desc_pub.ops.ops);
    dds_cdrstream_desc_from_topic_desc (&desc_sub, tests[i].desc_sub);
    assert (desc_sub.ops.ops);

    dds_ostream_t os = { .m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2 };
    uint8_t *sample_pub = ddsrt_malloc (desc_pub.size);
    memset (sample_pub, 0xef, desc_pub.size); // assumes no pointers (strings, sequences, @external, @optional) in pub type
    bool ret = dds_stream_write_sample (&os, &dds_cdrstream_default_allocator, sample_pub, &desc_pub);
    CU_ASSERT_FATAL (ret);

    uint8_t *sample_sub = ddsrt_malloc (desc_sub.size);
    memset (sample_sub, 0xbe, desc_sub.size);
    tests[i].init_sub (sample_sub);
    dds_istream_t is = { .m_buffer = os.m_buffer, .m_index = 0, .m_size = os.m_size, .m_xcdr_version = os.m_xcdr_version };
    dds_stream_read_sample (&is, sample_sub, &dds_cdrstream_default_allocator, &desc_sub);
    tests[i].check_sub (sample_sub);

    // clean-up
    dds_ostream_fini (&os, &dds_cdrstream_default_allocator);
    ddsrt_free (sample_pub);
    dds_stream_free_sample (sample_sub, &dds_cdrstream_default_allocator, desc_sub.ops.ops);
    ddsrt_free (sample_sub);

    dds_cdrstream_desc_fini (&desc_pub, &dds_cdrstream_default_allocator);
    dds_cdrstream_desc_fini (&desc_sub, &dds_cdrstream_default_allocator);
  }
}
#undef D

#define VAR (DDS_FIXED_KEY_MAX_SIZE + 1)
#define D(n) (&CdrStreamKeySize_ ## n ## _desc)
CU_Test(ddsc_cdrstream, key_size)
{
  static const struct {
    const dds_topic_descriptor_t *desc;
    bool fixed_key_xcdr1;
    bool fixed_key_xcdr2;
    uint32_t keysz_xcdr1;
    uint32_t keysz_xcdr2;
    bool fixed_key_xcdrv2_keyhash;
  } tests[] = {
    { D(t1), true, true, 6, 6, true },   // key size: 4 + 2
    { D(t2), false, true, VAR, 14, true },  // key size: 1 + 7/3 (pad) + 8 + 2
    { D(t3), false, true, VAR, 14, true },  // key size: 1 + 7/3 (pad) + 8 + 2
    { D(t4), false, true, VAR, 13, true },  // key size: 1 + 1 (pad) + 2 + 4/0 (pad) + 8 + 1
    { D(t5), false, false, VAR, VAR, false },
    { D(t6), true, true, 16, 16, true }, // key size: 8 + 1 + 3 (pad) + 4
    { D(t7), false, true, VAR, 16, true }, // key size: 2 + 6/2 (pad) + 8 + 1 + 1 (pad) 2
    { D(t8), true, true, 15, 15, true },
    { D(t9), true, true, 12, 12, true },
    { D(t10), true, true, 8, 8, true }, // key size: 8
    { D(t11), true, true, 16, 16, true }, // key size: 8 + 8
    { D(t12), true, true, 16, 16, true }, // key size: 1 + 3 (pad) + 8 + 4
    { D(t13), true, true, 16, 16, true }, // key size XCDR1: 4 + 1 + 3 (pad) + 8 / XCDR2: 8 + 1 + 3 (pad) + 4
    { D(t14), false, true, VAR, 16, true }, // key size XCDR1: 1 + 7 (pad) + 8 + 4 / XCDR2: 1 + 3 (pad) + 8 + 4
    { D(t15), true, true, 12, 12, false }, // key size XCDR1: 1 + 1 + 1 + 1 (pad) + 4 + 4 / XCDR2: 1 + 1 + 1 + 1 (pad) + 4 + 4 / XCDR2_KH: 1 + 3 (pad) + 4 + 1 + 3 (pad) + 4 + 1
    { D(t16), true, true, 4, 4, true }, // key size: 4
    { D(t17), true, true, 1, 1, true }, // key size: 1
    { D(t18), true, true, 10, 16, true }, // key size XCDR1: 1 + 1 (pad) + 4 * 2 / XCDR2: 1 + 3 (pad) + 4 (dheader) + 4 * 2
    { D(t19), true, true, 4, 4, true }, // key size: 4
    { D(t20), true, true, 1, 1, true }, // key size: 1
    { D(t21), true, true, 16, 16, true }, // key size XCDR1: 1 + 7 (pad) + 1 * 8 / XCDR2: 1 + 3 (pad) + 4 (dheader) + 1 * 8 /

    { D(t22), true, true, 4, 8, true }, // key size: XCDR1: 4 / XCDR2: 4 (dh) + 4
    { D(t23), false, true, 0, 12, true }, // key size: XCDR2: 4 (dh) + 4 (emh) + 4
    { D(t24), false, true, 0, 12, true }, // key size: XCDR2: 4 (dh) + 4 (dh) + 4
    { D(t25), false, false, 0, VAR, true }, // key size: XCDR2: 4 (dh) + 4 (emh) + 4 (dh) + 4 (emh) + 4
    { D(t26), false, false, 0, VAR, true }, // key size: XCDR2: 4 (dh) + 4 (emh) + 4 (emh-nextint) + 4 (dh) + 1
    { D(t27), false, true, 0, 9, true }, // key size: XCDR2: 4 (dh) + 4 (dh) + 1
    { D(t28), false, true, 0, 14, true }, // key size: XCDR2: 4 (dh) + 4 (emh) + 4 (dh) + 2 * 1
    { D(t29), false, true, 0, 12, true }, // key size: XCDR2: 4 (dh) + 4 (emh) + 4

    { D(t30), false, false, VAR, VAR, false },
    { D(t31), true, true, 16, 16, true }, // key size: 4 (length) + 3 * 4
    { D(t32), true, true, 11, 11, true }, // key size: 1 + 3 (pad) + 4 (length) + 3 * 1
    { D(t33), true, true, 6, 10, true }, // key size: XCDR1: 4 (length) + 2 * 1 / XCDR2: 4 (dh) + 4 (length) + 2 * 1
    { D(t34), false, false, VAR, VAR, false },
    { D(t35), false, true, 0, 13, true }, // key size: XCDR2: 4 (dh) + 4 (length) + 4 (dh) + 1
    { D(t36), true, true, 16, 16, true }, // key size: 4 (length) + 12 * 1
    { D(t37), false, true, 0, 16, true }, // key size: XCDR2: 4 (dh) + 4 (emh) + 4 (length) + 4
    { D(t38), true, true, 12, 16, true }, // key size: XCDR1: 4 (length) + 2 * (1 + 1 (pad) + 2) / XCDR2: 4 (dh) + 4 (length) + 2 * (1 + 1 (pad) + 2)

    { D(t39), true, true, 2, 6, true }, // key size: XCDR1: 2 * 1 / XCDR2: 4 (dh) + 2 * 1
    { D(t40), true, true, 4, 8, true }, // key size: XCDR1: 4 * 1 / XCDR2: 4 (dh) + 4 * 1
    { D(t41), false, true, 0, 9, true }, // key size: XCDR2: 4 (dh) + 4 (dh) + 1
    { D(t42), false, true, 0, 16, true }, // key size: XCDR2: 4 (dh) + 4 (emh) + 4 (dh) + 4
    { D(t43), true, true, 8, 12, true }, // key size: XCDR1: 2 * (1 + 1 (pad) + 2) / XCDR2: 4 (dh) + 2 * (1 + 1 (pad) + 2)

    { D(t44), false, false, VAR, VAR, false },
    { D(t45), false, false, VAR, VAR, false },
    { D(t46), false, false, VAR, VAR, false },
  };

  for (size_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++) {
    printf ("running test for type: %s\n", tests[i].desc->m_typename);

    uint32_t keysz_xcdrv1 = 0, keysz_xcdrv2 = 0;
    struct dds_cdrstream_desc desc;
    dds_cdrstream_desc_from_topic_desc (&desc, tests[i].desc);
    uint32_t key_flags = dds_stream_key_flags (&desc, &keysz_xcdrv1, &keysz_xcdrv2);
    CU_ASSERT_EQUAL_FATAL ((key_flags & DDS_TOPIC_FIXED_KEY) != 0, tests[i].fixed_key_xcdr1);
    CU_ASSERT_EQUAL_FATAL ((key_flags & DDS_TOPIC_FIXED_KEY_XCDR2) != 0, tests[i].fixed_key_xcdr2);
    CU_ASSERT_EQUAL_FATAL (keysz_xcdrv1, tests[i].keysz_xcdr1);
    CU_ASSERT_EQUAL_FATAL (keysz_xcdrv2, tests[i].keysz_xcdr2);
    CU_ASSERT_EQUAL_FATAL ((key_flags & DDS_TOPIC_FIXED_KEY_XCDR2_KEYHASH) != 0, tests[i].fixed_key_xcdrv2_keyhash);
    dds_cdrstream_desc_fini (&desc, &dds_cdrstream_default_allocator);
  }
}
#undef VAR
#undef D

#define D(n) (&CdrStreamKeyExt_ ## n ## _desc)
CU_Test(ddsc_cdrstream, key_flags_ext)
{
  static const struct {
    const dds_topic_descriptor_t *desc;
    bool key_appendable;
    bool key_mutable;
  } tests[] = {
    { D(t1), false, false },
    { D(t1a), false, false },
    { D(t2), true, false },
    { D(t3), false, true },
    { D(t4), false, false },
    { D(t4a), true, false },
    { D(t4b), true, false },
    { D(t5), false, false },
    { D(t5a), false, true },
    { D(t5b), false, true },
    { D(t6), true, false },
    { D(t6a), true, true },
    { D(t6b), true, true },
  };

  for (size_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++) {
    printf ("running test for type: %s\n", tests[i].desc->m_typename);
    struct dds_cdrstream_desc desc;
    dds_cdrstream_desc_from_topic_desc (&desc, tests[i].desc);
    uint32_t key_flags = dds_stream_key_flags (&desc, NULL, NULL);
    CU_ASSERT_EQUAL_FATAL ((key_flags & DDS_TOPIC_KEY_APPENDABLE) != 0, tests[i].key_appendable);
    CU_ASSERT_EQUAL_FATAL ((key_flags & DDS_TOPIC_KEY_MUTABLE) != 0, tests[i].key_mutable);
    dds_cdrstream_desc_fini (&desc, &dds_cdrstream_default_allocator);
  }
}
#undef D

typedef struct MutStructSeq
{
  dds_sequence_long b;
  uint8_t c;
} MutStructSeq;

typedef struct ExternMutStructSeq
{
  struct MutStructSeq * x;
} ExternMutStructSeq;

static const uint32_t ExternMutStructSeq_ops [] =
{
  /* ExternMutStructSeq */
  DDS_OP_DLC,
  DDS_OP_ADR | DDS_OP_FLAG_OPT | DDS_OP_FLAG_EXT | DDS_OP_TYPE_EXT, offsetof (ExternMutStructSeq, x), (4u << 16u) + 5u /* MutStructSeq */, sizeof (MutStructSeq),
  DDS_OP_RTS,

  /* MutStructSeq */
  DDS_OP_PLC,
  DDS_OP_PLM | 5, 1u,
  DDS_OP_PLM | 6, 2u,
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_4BY | DDS_OP_FLAG_SGN, offsetof (MutStructSeq, b),
  DDS_OP_RTS,
  DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (MutStructSeq, c),
  DDS_OP_RTS
};

CU_Test(ddsc_cdrstream, init_sequence_in_external_struct)
{
  static uint8_t cdr[] = {
    0x0d, 0x00, 0x00, 0x00, // 13 bytes follow for ExternMutStructSeq
    0x01, 0x00, 0x00, 0x00, // optional member present + 3x pad
    0x05, 0x00, 0x00, 0x00, // 5 bytes follow for MutStructSeq
    0x02, 0x00, 0x00, 0x00, // EM: id=2, length code 0 = 1B
    0x7b                    // 123: magic value for "c"
  };
  struct dds_cdrstream_desc descr;
  memset (&descr, 0, sizeof (descr));
  dds_cdrstream_desc_init (&descr, &dds_cdrstream_default_allocator, sizeof (ExternMutStructSeq), dds_alignof (ExternMutStructSeq), 0, ExternMutStructSeq_ops, NULL, 0, 0);
  uint32_t actual_size;
  const bool byteswap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
  const bool norm_ok = dds_stream_normalize (cdr, sizeof (cdr), byteswap, DDSI_RTPS_CDR_ENC_VERSION_2, &descr, false, &actual_size);
  CU_ASSERT_FATAL (norm_ok && actual_size == sizeof (cdr));
  dds_istream_t is;
  dds_istream_init (&is, sizeof (cdr), cdr, DDSI_RTPS_CDR_ENC_VERSION_2);
  ExternMutStructSeq * sample = ddsrt_calloc (1, sizeof (*sample));
  dds_stream_read_sample (&is, sample, &dds_cdrstream_default_allocator, &descr);
  dds_stream_free_sample (sample, &dds_cdrstream_default_allocator, descr.ops.ops);
  ddsrt_free (sample);
  dds_cdrstream_desc_fini (&descr, &dds_cdrstream_default_allocator);
}


#define D(n) (&CdrStreamChecking_ ## n ## _desc)
#define C(n) &(CdrStreamChecking_ ## n)
CU_Test (ddsc_cdrstream, check_write_reject)
{
  // Most are for checking it rejects something, but for example with @external
  // it needs to accept null pointers in some cases.  Hence the handful of cases
  // that are expected to result in correct CDR
  const union { CdrStreamChecking_en2 u; int i; } out_of_range_enum = { .i = 1 };
  const struct {
    const dds_topic_descriptor_t *desc;
    const void *sample;
    const char *description;
    uint32_t cdrsize_if_ok;
    const uint8_t *cdr_if_ok;
  } tests[] = {
    { D(t1), C(t1){.f1={._length=2,._buffer=(uint8_t[]){1,2}}}, "oversize sequence" },
    { D(t1), C(t1){.f1={._length=1,._buffer=NULL}}, "non-empty sequence with null pointer" },
    { D(t2), C(t2){.f1=out_of_range_enum.u}, "out-of-range enum" },
    { D(t3), C(t3){.f1=2}, "out-of-range bitmask" },
    { D(t4), C(t4){.f1=NULL}, "@external w/ null pointer" },
    { D(t4a), C(t4a){.f1=NULL}, "@external @optional w/ null pointer", 1, (uint8_t[]){0} },
    { D(t4b), C(t4b){.f1=NULL}, "@external string w/ null pointer", 5, (uint8_t[]){SER32(1),0} },
    { D(t5), C(t5){.f1={._d=0,._u={.c0=NULL}}}, "union with @external w/ null pointer" },
    { D(t5), C(t5){.f1={._d=1,._u={.c1=NULL}}}, "union with @external string w/ null pointer", 9, (uint8_t[]){1, 0,0,0, SER32(1),0} },
    { D(t6), C(t6x){.f1=0}, "boolean 0", 1, (uint8_t[]){0} },
    { D(t6), C(t6x){.f1=1}, "boolean 1", 1, (uint8_t[]){1} },
    { D(t6), C(t6x){.f1=2}, "boolean 2", 1, (uint8_t[]){1} },
    { D(t6), C(t6x){.f1=255}, "boolean 255", 1, (uint8_t[]){1} },
    { D(t7), C(t7x){.f1={._d=0,._u={.c1=3}}}, "disc bool 0", 1, (uint8_t[]){0} },
    { D(t7), C(t7x){.f1={._d=1,._u={.c1=3}}}, "disc bool 1", 2, (uint8_t[]){1,1} },
    { D(t7), C(t7x){.f1={._d=2,._u={.c1=3}}}, "disc bool 2", 2, (uint8_t[]){1,1} },
    { D(t7), C(t7x){.f1={._d=255,._u={.c1=3}}}, "disc bool 255", 2, (uint8_t[]){1,1} },
    { D(t8), C(t8x){.f1={0,0}}, "boolean arr 0", 2, (uint8_t[]){0,0} },
    { D(t8), C(t8x){.f1={1,1}}, "boolean arr 1", 2, (uint8_t[]){1,1} },
    { D(t8), C(t8x){.f1={1,2}}, "boolean arr 2", 2, (uint8_t[]){1,1} },
    { D(t8), C(t8x){.f1={255,2}}, "boolean arr 255", 2, (uint8_t[]){1,1} }
  };

  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    const uint32_t xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2;
    printf("running test for desc %s: %s\n", tests[i].desc->m_typename, tests[i].description);

    struct dds_cdrstream_desc desc;
    dds_cdrstream_desc_from_topic_desc (&desc, tests[i].desc);
    assert (desc.ops.ops);

    size_t size = dds_stream_getsize_sample (tests[i].sample, &desc, xcdr_version);
    dds_ostream_t os = { .m_xcdr_version = xcdr_version };
    bool ret = dds_stream_write_sample (&os, &dds_cdrstream_default_allocator, tests[i].sample, &desc);
    CU_ASSERT_FATAL (ret == (tests[i].cdr_if_ok != NULL));
    if (tests[i].cdr_if_ok)
    {
      CU_ASSERT_FATAL (size == os.m_index);
      CU_ASSERT_FATAL (os.m_index == tests[i].cdrsize_if_ok);
      CU_ASSERT_FATAL (memcmp (tests[i].cdr_if_ok, os.m_buffer, os.m_index) == 0);
    }

    if (desc.keys.nkeys)
    {
      // Repeat with key serialization: type and data are so simple that the result should
      // be exactly the same as for the full sample. The point is to check that the key
      // serialization handling also handles these edge cases correctly.
      size = dds_stream_getsize_key (tests[i].sample, &desc, xcdr_version);
      os.m_index = 0;
      ret = dds_stream_write_key (&os, DDS_CDR_KEY_SERIALIZATION_SAMPLE, &dds_cdrstream_default_allocator, tests[i].sample, &desc);
      CU_ASSERT_FATAL (ret == (tests[i].cdr_if_ok != NULL));
      if (tests[i].cdr_if_ok)
      {
        CU_ASSERT_FATAL (size == os.m_index);
        CU_ASSERT_FATAL (os.m_index == tests[i].cdrsize_if_ok);
        CU_ASSERT_FATAL (memcmp (tests[i].cdr_if_ok, os.m_buffer, os.m_index) == 0);
      }
    }

    dds_ostream_fini (&os, &dds_cdrstream_default_allocator);
    dds_cdrstream_desc_fini (&desc, &dds_cdrstream_default_allocator);
  }
}
#undef C
#undef D


#define D(n) (&CdrStreamChecking_ ## n ## _desc)
CU_Test (ddsc_cdrstream, check_normalize_boolean)
{
  // Need to verify that stream_normalize cleans up the booleans
  const struct {
    const dds_topic_descriptor_t *desc;
    const char *description;
    uint32_t cdrsize;
    const uint8_t *cdr;
    const uint8_t *ncdr;
  } tests[] = {
    { D(t6), "boolean 0", 1, (uint8_t[]){0}, (uint8_t[]){0} },
    { D(t6), "boolean 1", 1, (uint8_t[]){1}, (uint8_t[]){1} },
    { D(t6), "boolean 2", 1, (uint8_t[]){2}, (uint8_t[]){1} },
    { D(t6), "boolean 255", 1, (uint8_t[]){255}, (uint8_t[]){1} },
    { D(t7), "disc bool 0", 1, (uint8_t[]){0}, (uint8_t[]){0} },
    // also check correct label is entered by checking it normalizes the bool there, too
    { D(t7), "disc bool 1", 2, (uint8_t[]){1,3}, (uint8_t[]){1,1} },
    { D(t7), "disc bool 2", 2, (uint8_t[]){2,3}, (uint8_t[]){1,1} },
    { D(t7), "disc bool 255", 2, (uint8_t[]){255,3}, (uint8_t[]){1,1} },
    { D(t8), "boolean arr 0", 2, (uint8_t[]){0,0}, (uint8_t[]){0,0} },
    { D(t8), "boolean arr 1", 2, (uint8_t[]){1,1}, (uint8_t[]){1,1} },
    { D(t8), "boolean arr 2", 2, (uint8_t[]){1,2}, (uint8_t[]){1,1} },
    { D(t8), "boolean arr 255", 2, (uint8_t[]){255,1}, (uint8_t[]){1,1} }
  };

  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    printf("running test for desc %s: %s\n", tests[i].desc->m_typename, tests[i].description);

    struct dds_cdrstream_desc desc;
    dds_cdrstream_desc_from_topic_desc (&desc, tests[i].desc);
    assert (desc.ops.ops);

    void *cdr = ddsrt_memdup (tests[i].cdr, tests[i].cdrsize);
    uint32_t act_size;
    bool ret = dds_stream_normalize (cdr, tests[i].cdrsize, false, DDSI_RTPS_CDR_ENC_VERSION_2, &desc, false, &act_size);
    CU_ASSERT_FATAL (ret && act_size == tests[i].cdrsize);
    CU_ASSERT_FATAL (memcmp (cdr, tests[i].ncdr, tests[i].cdrsize) == 0);
    if (desc.keys.nkeys)
    {
      ret = dds_stream_normalize (cdr, tests[i].cdrsize, true, DDSI_RTPS_CDR_ENC_VERSION_2, &desc, false, &act_size);
      CU_ASSERT_FATAL (ret && act_size == tests[i].cdrsize);
      CU_ASSERT_FATAL (memcmp (cdr, tests[i].ncdr, tests[i].cdrsize) == 0);
    }
    ddsrt_free (cdr);
    dds_cdrstream_desc_fini (&desc, &dds_cdrstream_default_allocator);
  }
}
#undef D

static bool eq_CdrStreamWstring_t1 (const void *va, const void *vb)
{
  const CdrStreamWstring_t1 *a = va;
  const CdrStreamWstring_t1 *b = vb;
  return wcscmp (a->ws, b->ws) == 0 && a->k == b->k;
}

static bool eq_CdrStreamWstring_t2 (const void *va, const void *vb)
{
  const CdrStreamWstring_t2 *a = va;
  const CdrStreamWstring_t2 *b = vb;
  return wcscmp (a->ws1, b->ws1) == 0 && wcscmp (a->ws2, b->ws2) == 0 && a->k == b->k;
}

static bool eq_CdrStreamWstring_t3 (const void *va, const void *vb)
{
  const CdrStreamWstring_t3 *a = va;
  const CdrStreamWstring_t3 *b = vb;
  return wcscmp (a->ws1a[0], b->ws1a[0]) == 0 && wcscmp (a->ws1a[1], b->ws1a[1]) == 0 && a->k == b->k;
}

static bool eq_CdrStreamWstring_t4 (const void *va, const void *vb)
{
  const CdrStreamWstring_t4 *a = va;
  const CdrStreamWstring_t4 *b = vb;
  if (a->ws1s._length != b->ws1s._length)
    return false;
  for (uint32_t i = 0; i < a->ws1s._length; i++)
    if (wcscmp (a->ws1s._buffer[i], b->ws1s._buffer[i]) != 0)
      return false;
  if (a->ws1bs._length != b->ws1bs._length)
    return false;
  for (uint32_t i = 0; i < a->ws1bs._length; i++)
    if (wcscmp (a->ws1bs._buffer[i], b->ws1bs._buffer[i]) != 0)
      return false;
  if (a->k != b->k)
    return false;
  return true;
}

static bool eq_CdrStreamWstring_t5 (const void *va, const void *vb)
{
  const CdrStreamWstring_t5 *a = va;
  const CdrStreamWstring_t5 *b = vb;
  if (a->u._d != b->u._d)
    return false;
  switch (a->u._d)
  {
    case 1:
      if (wcscmp (a->u._u.ws, b->u._u.ws) != 0)
        return false;
      break;
    case 2:
      if (wcscmp (a->u._u.ws1, b->u._u.ws1) != 0)
        return false;
      break;
    case 3:
      if (a->u._u.wss._length != b->u._u.wss._length)
        return false;
      for (uint32_t i = 0; i < a->u._u.wss._length; i++)
        if (wcscmp (a->u._u.wss._buffer[i], b->u._u.wss._buffer[i]) != 0)
          return false;
      break;
    case 4:
      if (a->u._u.ws1bs._length != b->u._u.ws1bs._length)
        return false;
      for (uint32_t i = 0; i < a->u._u.ws1bs._length; i++)
        if (wcscmp (a->u._u.ws1bs._buffer[i], b->u._u.ws1bs._buffer[i]) != 0)
          return false;
      break;
    case 5:
      if (wcscmp (a->u._u.ws1a[0], b->u._u.ws1a[0]) != 0)
        return false;
      if (wcscmp (a->u._u.ws1a[1], b->u._u.ws1a[1]) != 0)
        return false;
      break;
    case 6:
      for (uint32_t j = 0; j < 2; j++)
      {
        if (a->u._u.ws1abs[j]._length != b->u._u.ws1abs[j]._length)
          return false;
        for (uint32_t i = 0; i < a->u._u.ws1abs[j]._length; i++)
          if (wcscmp (a->u._u.ws1abs[j]._buffer[i], b->u._u.ws1abs[j]._buffer[i]) != 0)
            return false;
      }
      break;
    default:
      break;
  }
  if (a->k != b->k)
    return false;
  return true;
}

#define IDENT(x_) x_
#define FIRST(w_, v_) w_
#define PLUS() +
#define MAKE_SER(w_, v_) SER##w_(v_)
#define COMMA() ,
#define SERSIZE(...) ((DDSRT_FOREACH_PAIR_WRAP (FIRST, PLUS, __VA_ARGS__)) / 8)
#define CDR(...) SERSIZE(__VA_ARGS__), (uint8_t[]){ DDSRT_FOREACH_PAIR_WRAP (MAKE_SER, COMMA, __VA_ARGS__) }

#define UTF16(x_) 16,x_
#define WSTR0 32,0
#define WSTR(...) 32,(2*DDSRT_COUNT_ARGS(__VA_ARGS__)), DDSRT_FOREACH_WRAP(UTF16, COMMA, __VA_ARGS__)
#define PAD2 16,0
#define DHDR(...) 32,(SERSIZE(__VA_ARGS__)), __VA_ARGS__

#define CSEQ0 { ._length = 0, ._buffer = NULL }
#define CSEQ(type_, ...) { \
    ._length = DDSRT_COUNT_ARGS(__VA_ARGS__), \
    ._buffer = (type_ *)&(type_[]){ DDSRT_FOREACH_WRAP (IDENT, COMMA, __VA_ARGS__) } \
  }
#define WSS(...) CSEQ(wchar_t *, __VA_ARGS__)
#define WSSB(...) CSEQ(CdrStreamWstring_wstring1, __VA_ARGS__)

#define D(n, ...) (&CdrStreamWstring_ ## n ## _desc), eq_CdrStreamWstring_ ## n, (&(CdrStreamWstring_ ## n){ __VA_ARGS__ })
CU_Test (ddsc_cdrstream, check_wstring_valid)
{
  const struct {
    const dds_topic_descriptor_t *desc;
    bool (*eq) (const void *a, const void *b);
    const void *data;
    uint32_t cdrsize;
    const uint8_t *cdr;
  } tests[] = {
    /* 0 */
    { D(t1, L"",   2),  CDR(32,0, 32,2) },
    { D(t1, L"a",  3),  CDR(WSTR('a'), PAD2, 32,3) },
    { D(t1, L"ab", 5),  CDR(WSTR('a','b'), 32,5) },
    { D(t2, L"",   L"",    2), CDR(WSTR0, WSTR0, 32,2) },
    { D(t2, L"a",  L"",    3), CDR(WSTR('a'), PAD2, WSTR0, 32,3) },
    /* 5 */
    { D(t2, L"",   L"c",   5), CDR(WSTR0, WSTR('c'), PAD2, 32,5) },
    { D(t2, L"",   L"cd",  7), CDR(WSTR0, WSTR('c','d'), 32,7) },
    { D(t2, L"a",  L"c",  11), CDR(WSTR('a'), PAD2, WSTR('c'), PAD2, 32,11) },
    { D(t2, L"a",  L"cd", 13), CDR(WSTR('a'), PAD2, WSTR('c','d'), 32,13) },
    { D(t3, {L"", L""},    2), CDR(DHDR(WSTR0, WSTR0), 32,2) },
    /* 10 */
    { D(t3, {L"a", L""},   3), CDR(DHDR(WSTR('a'), PAD2, WSTR0), 32,3) },
    { D(t3, {L"", L"c"},   5), CDR(DHDR(WSTR0, WSTR('c')), PAD2, 32,5) },
    { D(t3, {L"a", L"c"},  7), CDR(DHDR(WSTR('a'), PAD2, WSTR('c')), PAD2, 32,7) },
    { D(t4, CSEQ0, CSEQ0, 2), CDR(DHDR(32,0), DHDR(32,0), 32,2) },
    { D(t4, WSS(L"a"), CSEQ0, 3), CDR(DHDR(32,1, WSTR('a')), PAD2, DHDR(32,0), 32,3) },
    /* 15 */
    { D(t4, WSS(L"a", L"b"), CSEQ0, 5), CDR(DHDR(32,2, WSTR('a'), PAD2, WSTR('b')), PAD2, DHDR(32,0), 32,5) },
    { D(t4, CSEQ0, WSSB(L"c"), 7), CDR(DHDR(32,0), DHDR(32,1, WSTR('c')), PAD2, 32,7) },
    { D(t4, CSEQ0, WSSB(L"c",L"d"), 11), CDR(DHDR(32,0), DHDR(32,2, WSTR('c'), PAD2, WSTR('d')), PAD2, 32,11) },
    { D(t4, WSS(L"a"), WSSB(L"c"), 13), CDR(DHDR(32,1, WSTR('a')), PAD2, DHDR(32,1, WSTR('c')), PAD2, 32,13) },
    { D(t4, WSS(L"a"), WSSB(L"c",L"d"), 17), CDR(DHDR(32,1, WSTR('a')), PAD2, DHDR(32,2, WSTR('c'), PAD2, WSTR('d')), PAD2, 32,17) },
    /* 20 */
    { D(t4, WSS(L"a", L"b"), WSSB(L"c"), 19), CDR(DHDR(32,2, WSTR('a'), PAD2, WSTR('b')), PAD2, DHDR(32,1, WSTR('c')), PAD2, 32,19) },
    { D(t4, WSS(L"a", L"b"), WSSB(L"c",L"d"), 23), CDR(DHDR(32,2, WSTR('a'), PAD2, WSTR('b')), PAD2, DHDR(32,2, WSTR('c'), PAD2, WSTR('d')), PAD2, 32,23) },
    { D(t5, {0}, 2), CDR(32,0, 32,2) },
    { D(t5, {1,{.ws=L"abcd"}}, 3), CDR(32,1, WSTR('a','b','c','d'), 32,3) },
    { D(t5, {2,{.ws1=L"a"}}, 5), CDR(32,2, WSTR('a'), PAD2, 32,5) },
    /* 25 */
    { D(t5, {3,{.wss=CSEQ0}}, 7), CDR(32,3, DHDR(WSTR0), 32,7) },
    { D(t5, {3,{.wss=WSS(L"a",L"b")}}, 11), CDR(32,3, DHDR(32,2, WSTR('a'), PAD2, WSTR('b')), PAD2, 32,11) },
    { D(t5, {4,{.ws1bs=WSSB(L"a",L"b")}}, 13), CDR(32,4, DHDR(32,2, WSTR('a'), PAD2, WSTR('b')), PAD2, 32,13) },
    { D(t5, {5,{.ws1a={L"a",L"b"}}}, 17), CDR(32,5, DHDR(WSTR('a'), PAD2, WSTR('b')), PAD2, 32,17) },
    { D(t5, {6,{.ws1abs={WSSB(L"a",L"b"),WSSB(L"c",L"d")}}}, 19),
      CDR(32,6,
        DHDR(DHDR(32,2, WSTR('a'), PAD2, WSTR('b')), PAD2,
             DHDR(32,2, WSTR('c'), PAD2, WSTR('d'))),
        PAD2, 32,19) },
  };

  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    printf("running test %"PRIu32" for desc %s\n", i, tests[i].desc->m_typename);

    struct dds_cdrstream_desc desc;
    dds_cdrstream_desc_from_topic_desc (&desc, tests[i].desc);
    assert (desc.ops.ops);

    dds_ostream_t os;
    dds_ostream_init (&os, &dds_cdrstream_default_allocator, 0, DDSI_RTPS_CDR_ENC_VERSION_2);
    const bool wok = dds_stream_write (&os, &dds_cdrstream_default_allocator, NULL, tests[i].data, desc.ops.ops);
    CU_ASSERT_FATAL (wok);

    CU_ASSERT_FATAL (os.m_index == tests[i].cdrsize);
    CU_ASSERT_FATAL (memcmp (os.m_buffer, tests[i].cdr, tests[i].cdrsize) == 0);

    uint32_t act_size;
    const bool nok = dds_stream_normalize (os.m_buffer, tests[i].cdrsize, false, DDSI_RTPS_CDR_ENC_VERSION_2, &desc, false, &act_size);
    CU_ASSERT_FATAL (nok);
    CU_ASSERT_FATAL (act_size == tests[i].cdrsize);
    CU_ASSERT_FATAL (memcmp (os.m_buffer, tests[i].cdr, tests[i].cdrsize) == 0); // nothing should've changed

    dds_istream_t is;
    dds_ostream_t osk;
    dds_istream_init (&is, os.m_index, os.m_buffer, os.m_xcdr_version);
    dds_ostream_init (&osk, &dds_cdrstream_default_allocator, 0, DDSI_RTPS_CDR_ENC_VERSION_2);
    const bool kok = dds_stream_extract_key_from_data (&is, &osk, &dds_cdrstream_default_allocator, &desc);
    CU_ASSERT_FATAL (kok);
    // key is a 32-bit int at the end, so need to consume all input and result must match tail of expected CDR
    CU_ASSERT_FATAL (is.m_index == os.m_index);
    CU_ASSERT_FATAL (osk.m_index == 4 && memcmp (osk.m_buffer, tests[i].cdr + tests[i].cdrsize - 4, 4) == 0);
    dds_ostream_fini (&osk, &dds_cdrstream_default_allocator);

    dds_istream_init (&is, os.m_index, os.m_buffer, os.m_xcdr_version);
    void *data = dds_alloc (desc.size);
    dds_stream_read (&is, data, &dds_cdrstream_default_allocator, desc.ops.ops);
    CU_ASSERT_FATAL (tests[i].eq (tests[i].data, data));
    dds_stream_free_sample (data, &dds_cdrstream_default_allocator, desc.ops.ops);
    dds_free (data);

    dds_ostream_fini (&os, &dds_cdrstream_default_allocator);
    dds_cdrstream_desc_fini (&desc, &dds_cdrstream_default_allocator);
  }
}
#undef D

CU_Test (ddsc_cdrstream, check_wstring_normalize)
{
  // all illegal inputs, no need to worry much about unbounded/bounded, arrays or sequences:
  // they all pass through the same functions, so "t2" suffices
  const struct {
    uint32_t cdrsize;
    const uint8_t *cdr;
  } tests[] = {
    { CDR(32,2) }, // insufficient data
    { CDR(32,1, 8,0, 8,0,8,0,8,0, 32,0) }, // odd length
    { CDR(32,2, 16,0xc800, PAD2, 32,0) }, // unpaired surrogate
    { CDR(32,2, 16,0xefff, PAD2, 32,0) }, // unpaired
    { CDR(32,4, 16,0xc800, 16,0xc800, 32,0) }, // unpaired
    { CDR(32,4, 16,0xe000, 16,0xe000, 32,0) }, // unpaired
    { CDR(32,4, 16,0xc800, 16,0xe000, 32,0) }, // wrong order
    { CDR(WSTR('a','b'), WSTR('a','b')) } // oversize bounded string
  };

  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    printf("running test %"PRIu32" \n", i);
    struct dds_cdrstream_desc desc;
    dds_cdrstream_desc_from_topic_desc (&desc, &CdrStreamWstring_t2_desc);
    assert (desc.ops.ops);
    dds_ostream_t os;
    dds_ostream_init (&os, &dds_cdrstream_default_allocator, 0, DDSI_RTPS_CDR_ENC_VERSION_2);
    uint32_t act_size;
    void *cdr = ddsrt_memdup (tests[i].cdr, tests[i].cdrsize);
    const bool nok = dds_stream_normalize (cdr, tests[i].cdrsize, false, DDSI_RTPS_CDR_ENC_VERSION_2, &desc, false, &act_size);
    CU_ASSERT_FATAL (!nok);
    ddsrt_free (cdr);
    dds_cdrstream_desc_fini (&desc, &dds_cdrstream_default_allocator);
  }
}
