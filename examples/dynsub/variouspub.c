#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "dds/dds.h"
#include "variouspub_types.h"

static void *samples_a[] = {
  &(A){ "Mariken",   "Wie sidi, vrient?", 0 },
  &(A){ "Die duvel", "Een meester vol consten,", 0 },
  &(A){ "Die duvel", "Nieuwers af falende, wes ic besta.", 0 },
  &(A){ "Mariken",   "'t Comt mi alleleens met wien dat ick ga,", 0 },
  &(A){ "Mariken",   "Also lief gae ic metten quaetsten als metten besten.", 0 },
  NULL
};

static void *samples_b[] = {
  &(B){ {"Die duvel", "Wildi u liefde te mi werts vesten,", 0},
        { ._length = 2, ._maximum = 2, ._release = false,
          ._buffer = (T[]){ {2,3},{5,7} } } },
  &(B){ {"Die duvel", "Ick sal u consten leeren sonder ghelijcke,", 0},
        { ._length = 3, ._maximum = 3, ._release = false,
          ._buffer = (T[]){ {11,13},{17,19},{23,29} } } },
  &(B){ {"Die duvel", "Die seven vrie consten: rethorijcke, musijcke,", 0},
        { ._length = 5, ._maximum = 5, ._release = false,
          ._buffer = (T[]){ {31,37},{41,43},{47,52},{59,61},{67,71} } } },
  &(B){ {"Die duvel", "Logica, gramatica ende geometrie,", 0},
        { ._length = 7, ._maximum = 7, ._release = false,
          ._buffer = (T[]){ {73,79},{83,89},{97,101},{103,107},{109,113},
                            {127,131},{137,139} } } },
  &(B){ {"Die duvel", "Aristmatica ende alkenie,", 0},
        { ._length = 11, ._maximum = 11, ._release = false,
          ._buffer = (T[]){ {149,151},{157,163},{167,173},{179,181},
                            {191,193},{197, 199},{211,223},{227,229},
                            {233,239},{241,251},{257,263} } } },
  NULL
};

static void *samples_c[] = {
  &(C){ { {"Die duvel", "Dwelc al consten sijn seer curable.", 0},
          { ._length = 13, ._maximum = 13, ._release = false,
            ._buffer = (T[]){ {269,271},{277,281},{283,293},{307,311},
                              {313,317},{331,337},{347,349},{353,359},
                              {367,373},{379,383},{389,397},{401,409},
                              {419,421} } } },
        8936 },
  &(C){ { {"Die duvel", "Noyt vrouwe en leefde op eerde so able", 0},
          { ._length = 17, ._maximum = 17, ._release = false,
            ._buffer = (T[]){ {431,433},{439,443},{449,457},{461,463},
                              {467,479},{487,491},{499,503},{509,521},
                              {523,541},{547,557},{563,569},{571,577},
                              {587,593},{599,601},{607,613},{617,619},
                              {631,641} } } },
        18088 },
  &(C){ { {"Die duvel", "Als ic u maken sal.", 0},
          { ._length = 19, ._maximum = 19, ._release = false,
            ._buffer = (T[]){ {643,647},{653,659},{661,673},{677,683},
                              {691,701},{709,719},{727,733},{739,743},
                              {751,757},{761,769},{773,787},{797,809},
                              {811,821},{823,827},{829,839},{853,857},
                              {859,863},{877,881},{883,887} } } },
        29172 },
  &(C){ { {"Mariken",  "So moetti wel zijn een constich man.", 0},
          { ._length = 23, ._maximum = 23, ._release = false,
            ._buffer = (T[]){ {907,911},{919,929},{937,941},{947,953},
                              {967,971},{977,983},{991,997},{1009,1013},
                              {1019,1021},{1031,1033},{1039,1049},{1051,1061},
                              {1063,1069},{1087,1091},{1093,1097},{1103,1109},
                              {1117,1123},{1129,1151},{1153,1163},{1171,1181},
                              {1187,1193},{1201,1213},{1217,1223} } } },
        16022 },
  &(C){ { {"Mariken",  "Wie sidi dan?", 0},
          { ._length = 29, ._maximum = 29, ._release = false,
            ._buffer = (T[]){ {1229,1231},{1237,1249},{1259,1277},{1279,1283},
                              {1289,1291},{1297,1301},{1303,1307},{1319,1321},
                              {1327,1361},{1367,1373},{1381,1399},{1409,1423},
                              {1427,1429},{1433,1439},{1447,1451},{1453,1459},
                              {1471,1481},{1483,1487},{1489,1493},{1499,1511},
                              {1523,1531},{1543,1549},{1553,1559},{1567,1571},
                              {1579,1583},{1597,1601},{1607,1609},{1613,1619},
                              {1621,1627} } } },
        17880 },
  NULL
};

static struct tpentry {
  const char *name;
  const dds_topic_descriptor_t *descr;
  void **samples;
  size_t count_offset;
} tptab[] = {
  { "A", &A_desc, samples_a, offsetof (A, count) },
  { "B", &B_desc, samples_b, offsetof (B, a.count) },
  { "C", &C_desc, samples_c, offsetof (C, b.a.count) },
  { NULL, NULL, NULL, 0 }
};

static void usage (const char *argv0)
{
  fprintf (stderr, "usage: %s {", argv0);
  const char *sep = "";
  for (struct tpentry *tpentry = &tptab[0]; tpentry->name; tpentry++)
  {
    fprintf (stderr, "%s%s", sep, tpentry->name);
    sep = "|";
  }
  fprintf (stderr, "}\n");
  exit (2);
}

int main (int argc, char **argv)
{
  if (argc != 2)
    usage (argv[0]);
  struct tpentry *tpentry;
  for (tpentry = &tptab[0]; tpentry->name; tpentry++)
    if (strcmp (tpentry->name, argv[1]) == 0)
      break;
  if (tpentry->name == NULL)
    usage (argv[0]);

  const dds_entity_t participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant < 0)
  {
    fprintf (stderr, "dds_create_participant: %s\n", dds_strretcode (participant));
    return 1;
  }

  const dds_entity_t topic = dds_create_topic (participant, tpentry->descr, tpentry->name, NULL, NULL);
  const dds_entity_t writer = dds_create_writer (participant, topic, NULL, NULL);
  uint32_t sample_idx = 0;
  uint32_t count = 0;
  while (1)
  {
    dds_return_t ret = 0;
    void *sample = tpentry->samples[sample_idx];
    uint32_t *countp = (uint32_t *) ((unsigned char *) sample + tpentry->count_offset);
    *countp = count++;
    if ((ret = dds_write (writer, sample)) < 0)
    {
      fprintf (stderr, "dds_write: %s\n", dds_strretcode (ret));
      dds_delete (participant);
      return 1;
    }
    if (tpentry->samples[++sample_idx] == NULL)
    {
      sample_idx = 0;
    }
    dds_sleepfor (DDS_SECS (1));
  }

  dds_delete (participant);
  return 0;
}
