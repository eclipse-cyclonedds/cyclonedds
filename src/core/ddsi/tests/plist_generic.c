// Copyright(c) 2019 to 2020 ZettaScale Technology and others
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
#include "dds/ddsrt/endian.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "ddsi__plist_generic.h"
#include "mem_ser.h"

struct desc {
  const enum ddsi_pserop desc[20];
  const void *data;
  size_t exp_sersize;
  const unsigned char *exp_ser;

  /* XbPROP means expectation after deser may be different from input, if exp_data
     is NULL, use "data", else use "exp_data" */
  const void *exp_data;
};

struct desc_invalid {
  const enum ddsi_pserop desc[20];
  size_t sersize;
  const unsigned char *ser;
};

typedef unsigned char raw[];
typedef uint32_t raw32[];
typedef uint64_t raw64[];
typedef ddsi_octetseq_t oseq;

static struct desc descs[] = {
  { {XSTOP}, (raw){0}, 0, (raw){0} },
  { {XO,XSTOP}, &(oseq){0, NULL },       4, (raw){SER32(0)} },
  { {XO,XSTOP}, &(oseq){1, (raw){3} },   5, (raw){SER32(1), 3} },
  { {XS,XSTOP}, &(char *[]){""},         5, (raw){SER32(1), 0} },
  { {XS,XSTOP}, &(char *[]){"meow"},     9, (raw){SER32(5), 'm','e','o','w',0} },
  { {XE1,XSTOP}, (raw32){1},             4, (raw){SER32(1)} },
  { {XE2,XSTOP}, (raw32){2},             4, (raw){SER32(2)} },
  { {XE3,XSTOP}, (raw32){3},             4, (raw){SER32(3)} },
  { {Xi,XSTOP},   (raw32){1},            4, (raw){SER32(1)} },
  { {Xix2,XSTOP}, (raw32){2,3},          8, (raw){SER32(2), SER32(3)} },
  { {Xix3,XSTOP}, (raw32){4,5,6},       12, (raw){SER32(4), SER32(5), SER32(6)} },
  { {Xix4,XSTOP}, (raw32){7,8,9,10},    16, (raw){SER32(7), SER32(8), SER32(9), SER32(10)} },
  { {Xu,XSTOP},   (raw32){1},            4, (raw){SER32(1)} },
  { {Xux2,XSTOP}, (raw32){2,3},          8, (raw){SER32(2), SER32(3)} },
  { {Xux3,XSTOP}, (raw32){4,5,6},       12, (raw){SER32(4), SER32(5), SER32(6)} },
  { {Xux4,XSTOP}, (raw32){7,8,9,10},    16, (raw){SER32(7), SER32(8), SER32(9), SER32(10)} },
  { {Xux5,XSTOP}, (raw32){7,8,9,10,11}, 20, (raw){SER32(7), SER32(8), SER32(9), SER32(10), SER32(11)} },
  { {Xl,XSTOP},   (raw64){123456789},    8, (raw){SER64(123456789)} },
  { {XD,XSTOP},   (uint64_t[]){314159265358979324},
    /* note: fractional part depends on rounding rule used for converting nanoseconds to NTP time
       Cyclone currently rounds up, so we have to do that too */
    8, (raw){SER32(314159265), SER32(1541804457)} },
  { {XD,XSTOP},   (uint64_t[]){DDS_NEVER},
    8, (raw){SER32(INT32_MAX), SER32(UINT32_MAX)} },
  { {XDx2,XSTOP}, (uint64_t[]){314159265358979324, 271828182845904524},
    16, (raw){SER32(314159265), SER32(1541804457), SER32(271828182), SER32(3633132267)} },
  { {Xo,XSTOP},   (raw){31},             1, (raw){31} },
  { {Xox2,XSTOP}, (raw){31,13},          2, (raw){31,13} },
  { {Xb,XSTOP},   (raw){1},              1, (raw){1} },
  { {Xbx2,XSTOP}, (raw){1,0},            2, (raw){1,0} },
  { {XG,XSTOP},   (raw32){3,4,5,0x1c1}, 16, (raw){SER32BE(3), SER32BE(4), SER32BE(5), SER32BE(0x1c1) } },
  { {XK,XSTOP},   (raw){1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16},
    16, (raw){1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16} },
  { {XQ,Xo,XSTOP,XSTOP}, &(oseq){3, (raw){1,2,3}},
    7, (raw){SER32(3), 1,2,3} },
  { {XQ,XS,XSTOP,XSTOP}, &(ddsi_stringseq_t){2, (char*[]){"tree","flower"}},
    27, (raw){SER32(2), SER32(5),'t','r','e','e',0, 0,0,0, SER32(7), 'f','l','o','w','e','r',0} },
  { {Xo,Xl,Xo,Xu,Xo,XSTOP},
    &(struct{unsigned char a;int64_t b;unsigned char c;uint32_t d;unsigned char e;}){ 1, 2, 3, 4, 5 },
     25, (raw){1,0,0,0,0,0,0,0,SER64(2),3,0,0,0,SER32(4),5} },
  { {Xo,XQ,Xo,Xu,Xo,XSTOP,Xo,XSTOP},
    &(struct{uint8_t b; oseq seq; uint8_t c;})
      {1, {2, (unsigned char *)(struct{uint8_t a; uint32_t b; uint8_t c;}[])
              { {0x10, 0x11, 0x12},
                {0x21, 0x22, 0x23} },
      }, 0x42 },
    26,
    (raw){1, /* pad */0,0,0, SER32(2),
             0x10, /* pad */0,0,0, SER32(0x11), 0x12,
             0x21, /* pad */0,0,   SER32(0x22), 0x23,
          0x42}
  },
  { {Xo,XQ,Xo,Xl,Xo,XSTOP,Xo,XSTOP},
    &(struct{uint8_t b; oseq seq; uint8_t c;})
      {1, {2, (unsigned char *)(struct{uint8_t a; int64_t b; uint8_t c;}[])
              { {0x10, 0x11, 0x12},
                {0x21, 0x22, 0x23} },
      }, 0x42 },
    42,
    (raw){1, /* pad */0,0,0, SER32(2),
             0x10, /* pad */0,0,0,0,0,0,0, SER64(0x11), 0x12,
             0x21, /* pad */0,0,0,0,0,0,   SER64(0x22), 0x23,
          0x42}
  },
  { {Xo,XQ,Xo,Xo,XQ,Xo,XSTOP,XSTOP,Xo,XSTOP},
    &(struct{uint8_t b; oseq seq; uint8_t c;})
      {1, {2, (unsigned char *)(struct{uint8_t a; uint8_t b; oseq seq;}[])
        { {0x10, 0x11, { 3, (unsigned char *)(struct{uint8_t a;}[]){ {'a'}, {'b'}, {'c'}}}},
          {0x21, 0x22, { 2, (unsigned char *)(struct{uint8_t a;}[]){ {'o'}, {'p'}}}}
        },
      }, 0x42 },
    31,
    (raw){1, /* pad */0,0,0, SER32(2),
             0x10, 0x11, /* pad */0,0,   SER32(3), 'a','b','c',
             0x21, 0x22, /* pad */0,0,0, SER32(2), 'o','p',
          0x42}
  },
  { {Xb,XQ,XbPROP,XS,Xo,XSTOP,XSTOP},
    &(struct{char b; oseq seq;}){1, {5, (unsigned char *)(struct{char b;char *s;uint8_t o;}[]){
      {0,"apple",1}, {1,"orange",2}, {0,"cherry",3}, {1,"fig",4}, {1,"prune",5}}}},
    43, (raw){1, 0,0,0, SER32(3),
      SER32(7), 'o','r','a','n','g','e',0, 2,
      SER32(4), 'f','i','g',0, 4, 0,0,0,
      SER32(6), 'p','r','u','n','e',0, 5
    },
    &(struct{char b; oseq seq;}){1, {3, (unsigned char *)(struct{char b;char *s;uint8_t o;}[]){
      {1,"orange",2}, {1,"fig",4}, {1,"prune",5}}}},
  },
  { {Xb,XQ,XbPROP,XS,Xo,XSTOP, Xopt,XQ,XbPROP,XS,Xo,XSTOP, XSTOP},
    &(struct{char b; oseq seq, seq2;}){1,
      {5, (unsigned char *)(struct{char b;char *s;uint8_t o;}[]){
        {0,"apple",1}, {1,"orange",2}, {0,"cherry",3}, {1,"fig",4}, {1,"prune",5}}},
      {2, (unsigned char *)(struct{char b;char *s;uint8_t o;}[]){
        {1,"oak",8}, {0,"beech",9}}}
    },
    57, (raw){1, 0,0,0,
      SER32(3),
      SER32(7), 'o','r','a','n','g','e',0, 2,
      SER32(4), 'f','i','g',0, 4, 0,0,0,
      SER32(6), 'p','r','u','n','e',0, 5,
      0,
      SER32(1),
      SER32(4), 'o','a','k',0, 8
    },
    &(struct{char b; oseq seq, seq2;}){1,
      {3, (unsigned char *)(struct{char b;char *s;uint8_t o;}[]){
        {1,"orange",2}, {1,"fig",4}, {1,"prune",5}}},
      {1,  (unsigned char *)(struct{char b;char *s;uint8_t o;}[]){
        {1,"oak",8}}}
    }
  }
};

CU_Test (ddsi_plist_generic, ser_and_deser)
{
  union {
    uint64_t u;
    void *p;
    char buf[256];
  } mem;

  for (size_t i = 0; i < sizeof (descs) / sizeof (descs[0]); i++)
  {
    size_t memsize;
    void *ser;
    size_t sersize;
    dds_return_t ret;
    ret = ddsi_plist_ser_generic (&ser, &sersize, descs[i].data, descs[i].desc);
    if (ret != DDS_RETCODE_OK)
      CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);
    if (sersize != descs[i].exp_sersize)
      CU_ASSERT (sersize == descs[i].exp_sersize);
    /* if sizes don't match, still check prefix */
    size_t cmpsize = (sersize < descs[i].exp_sersize) ? sersize : descs[i].exp_sersize;
    if (memcmp (ser, descs[i].exp_ser, cmpsize) != 0)
    {
      printf ("memcmp i = %zu\n", i);
      for (size_t k = 0; k < cmpsize; k++)
        printf ("  %3zu  %02x  %02x\n", k, ((unsigned char *)ser)[k], descs[i].exp_ser[k]);
      CU_ASSERT (!(bool)"memcmp");
    }
    /* check */
    memsize = ddsi_plist_memsize_generic (descs[i].desc);
    if (memsize > sizeof (mem))
      CU_ASSERT_FATAL (memsize <= sizeof (mem));
    /* memset to zero for used part so padding is identical to compiler inserted padding,
       but to something unlikely for the remainder */
    memset (mem.buf, 0, memsize);
    memset (mem.buf + memsize, 0xee, sizeof (mem) - memsize);

    /* 0 mod 4 is guaranteed by plist handling code, 0 mod 8 isn't, so be sure to check */
    ser = ddsrt_realloc (ser, sersize + 4);
    for (uint32_t shift = 0; shift != 4; shift += 4)
    {
      memmove ((char *) ser + shift, ser, sersize);
      ret = ddsi_plist_deser_generic (&mem, (char *) ser + shift, sersize, false, descs[i].desc);
      if (ret != DDS_RETCODE_OK)
        CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);
      /* the compare function should be happy with it */
      if (!ddsi_plist_equal_generic (descs[i].exp_data ? descs[i].exp_data : descs[i].data, &mem, descs[i].desc))
        CU_ASSERT (!(bool)"plist_equal_generic");
      /* content should be identical except when an XO, XS or XQ is present (because the first two
       alias the serialised form and XQ to freshly allocated memory), so we do a limited check */
      bool can_memcmp = true;
      for (const enum ddsi_pserop *op = descs[i].desc; *op != XSTOP && can_memcmp; op++)
        if (*op == XS || *op == XO || *op == XQ)
          can_memcmp = false;
      if (can_memcmp && memcmp (descs[i].exp_data ? descs[i].exp_data : descs[i].data, &mem, memsize) != 0)
        CU_ASSERT (!(bool)"memcmp");
      /* rely on mem checkers to find memory leaks, incorrect free, etc. */
      ddsi_plist_fini_generic (&mem, descs[i].desc, true);
    }
    ddsrt_free (ser);
  }
}

CU_Test (ddsi_plist_generic, unalias)
{
  union {
    uint64_t u;
    void *p;
    char buf[256];
  } mem;

  for (size_t i = 0; i < sizeof (descs) / sizeof (descs[0]); i++)
  {
    void *ser;
    size_t sersize;
    dds_return_t ret;
    (void) ddsi_plist_ser_generic (&ser, &sersize, descs[i].data, descs[i].desc);
    (void) ddsi_plist_deser_generic (&mem, ser, sersize, false, descs[i].desc);
    /* after unaliasing, the data should be valid even when the serialised form has been overwritten or freed */
    ret = ddsi_plist_unalias_generic (&mem, descs[i].desc);
    CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);
    memset (ser, 0xee, sersize);
    ddsrt_free (ser);
    if (!ddsi_plist_equal_generic (descs[i].exp_data ? descs[i].exp_data : descs[i].data, &mem, descs[i].desc))
      CU_ASSERT (!(bool)"plist_equal_generic");
    ddsi_plist_fini_generic (&mem, descs[i].desc, false);
  }
}

static struct desc_invalid descs_invalid[] = {
  { {Xb,XSTOP},   1, (raw){2} }, // 2 is not a valid boolean
  { {XS,XSTOP},   8, (raw){SER32(5), 'm','e','o','w',0} }, // short input
  { {XS,XSTOP},   8, (raw){SER32(4), 'm','e','o','w',0} }, // not terminated
  { {XG,XSTOP},  15, (raw){SER32BE(3), SER32BE(4), SER32BE(5), SER32BE(0x100) } }, // short input
  { {XK,XSTOP},  15, (raw){1,2,3,4,5,6,7,8,9,10,11,12,13,14,15} }, // short input
  { {XQ,Xo,XSTOP,XSTOP}, 7, (raw){SER32(4), 1,2,3} }, // short input
  { {XQ,XS,XSTOP,XSTOP}, // padding missing, short input
    24, (raw){SER32(2), SER32(5),'t','r','e','e',0, SER32(7), 'f','l','o','w','e','r',0} },
  { {Xb,XQ,XbPROP,XS,Xo,XSTOP,XSTOP},
    43, (raw){1, 0,0,0, SER32(3),
      SER32(7), 'o','r','a','n','g','e',0, 2,
      SER32(4), 'f','i','g',0, 4, 0,0,0,
      SER32(7), 'p','r','u','n','e',0, 5 // string not terminated
    } },
  { {Xb, XQ,XbPROP,XS,Xo,XSTOP, XQ,XbPROP,XS,Xo,XSTOP, XSTOP},
    43, (raw){1, 0,0,0,
      /* first sequence is valid */
      SER32(3),
      SER32(7), 'o','r','a','n','g','e',0, 2,
      SER32(4), 'f','i','g',0, 4, 0,0,0,
      SER32(6), 'p','r','u','n','e',0, 5,
      /* second sequence is invalid */
      0, /* pad */
      SER32(3),
      SER32(7), 'o','r','a','n','g','e',0, 2,
      SER32(4), 'f','i','g',0, 4, 0,0,0,
      SER32(7), 'p','r','u','n','e',0, 5 // string not terminated
    } },
  { {XQ,XQ,Xu,XSTOP,XSTOP}, 16, (raw){SER32(2),SER32(1),SER32(31415),SER32(3)} } // nested sequence failure
};

CU_Test (ddsi_plist_generic, invalid_input)
{
  union {
    uint64_t u;
    void *p;
    char buf[256];
  } mem;

  for (size_t i = 0; i < sizeof (descs_invalid) / sizeof (descs_invalid[0]); i++)
  {
    // plist handling guarantees 4-byte alignment, which (unsigned char[]) doesn't
    // also make sure to test try with 0 mod 8 & 4 mod 8 addresses
    // + 11: at most + 7 for aligning to 0 mod 8, + 4 for 4 mod 8
    // obviously one needs at most 7 but this is easier and does no harm
    char * const serbuf = ddsrt_malloc (descs_invalid[i].sersize + 11);
    for (size_t a = 0; a <= 4; a += 4)
    {
      char * const ser = serbuf + ((8 - ((uintptr_t)serbuf % 8)) % 8) + 4;
      memcpy (ser, descs_invalid[i].ser, descs_invalid[i].sersize);
      dds_return_t ret = ddsi_plist_deser_generic (&mem, ser, descs_invalid[i].sersize, false, descs_invalid[i].desc);
      if (ret == DDS_RETCODE_OK)
        CU_ASSERT_FATAL (ret != DDS_RETCODE_OK);
    }
    ddsrt_free (serbuf);
  }
}

CU_Test (ddsi_plist_generic, optional)
{
  union {
    uint64_t u;
    void *p;
    char buf[256];
  } mem;

  enum ddsi_pserop ser_desc[] = {Xb,XQ,XbPROP,XS,Xo,XSTOP,XSTOP};
  enum ddsi_pserop deser_desc[] = {Xb,XQ,XbPROP,XS,Xo,XSTOP, Xopt,XQ,XbPROP,XS,Xo,XSTOP, XSTOP};
  const void *data = &(struct{char b; oseq seq;}){
    1, {5, (unsigned char *)(struct{char b;char *s;uint8_t o;}[]){
      {0,"apple",1}, {1,"orange",2}, {0,"cherry",3}, {1,"fig",4}, {1,"prune",5}}}};
  size_t exp_sersize = 43;
  const unsigned char *exp_ser = (raw){
    1, 0,0,0, SER32(3),
    SER32(7), 'o','r','a','n','g','e',0, 2,
    SER32(4), 'f','i','g',0, 4, 0,0,0,
    SER32(6), 'p','r','u','n','e',0, 5
  };
  const void *exp_data = &(struct{char b; oseq seq; oseq seq2;}){
    1, {3, (unsigned char *)(struct{char b;char *s;uint8_t o;}[]){
      {1,"orange",2}, {1,"fig",4}, {1,"prune",5}}},
    {0, NULL}};

  size_t memsize;
  void *ser;
  size_t sersize;
  dds_return_t ret;
  ret = ddsi_plist_ser_generic (&ser, &sersize, data, ser_desc);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);
  CU_ASSERT (sersize == exp_sersize);
  /* if sizes don't match, still check prefix */
  size_t cmpsize = (sersize < exp_sersize) ? sersize : exp_sersize;
  if (memcmp (ser, exp_ser, cmpsize) != 0)
  {
    printf ("ddsi_plist_generic_optional: memcmp\n");
    for (size_t k = 0; k < cmpsize; k++)
      printf ("  %3zu  %02x  %02x\n", k, ((unsigned char *)ser)[k], exp_ser[k]);
    CU_ASSERT (!(bool)"memcmp");
  }
  /* check */
  memsize = ddsi_plist_memsize_generic (deser_desc);
  CU_ASSERT_FATAL (memsize <= sizeof (mem));
  memset (&mem, 0xee, sizeof (mem));
  ret = ddsi_plist_deser_generic (&mem, ser, sersize, false, deser_desc);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);
  /* the compare function should be happy with it */
  CU_ASSERT (ddsi_plist_equal_generic (exp_data, &mem, deser_desc));
  ddsi_plist_fini_generic (&mem, deser_desc, true);
  ddsrt_free (ser);
}
