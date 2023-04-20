// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause


// A C-program for MT19937, with initialization improved 2002/1/26.
// Coded by Takuji Nishimura and Makoto Matsumoto.
//
// Before using, initialize the state by using init_genrand(seed)
// or init_by_array(init_key, key_length).
//
// Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// 3. The names of its contributors may not be used to endorse or promote
//    products derived from this software without specific prior written
//    permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//
// Any feedback is very welcome.
// http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
// email: m-mat @ math.sci.hiroshima-u.ac.jp (remove space)

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "dds/ddsrt/random.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/static_assert.h"

#define N DDSRT_MT19937_N
#define M 397
#define MATRIX_A 0x9908b0dfU   /* constant vector a */
#define UPPER_MASK 0x80000000U /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffU /* least significant r bits */

static ddsrt_prng_t default_prng;
static ddsrt_mutex_t default_prng_lock;

/* initializes mt[N] with a seed */
static void init_genrand (ddsrt_prng_t *prng, uint32_t s)
{
  prng->mt[0] = s;
  for (prng->mti = 1; prng->mti < N; prng->mti++)
  {
    prng->mt[prng->mti] = (1812433253U * (prng->mt[prng->mti-1] ^ (prng->mt[prng->mti-1] >> 30)) + prng->mti);
    /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
    /* In the previous versions, MSBs of the seed affect   */
    /* only MSBs of the array mt[].                        */
    /* 2002/01/09 modified by Makoto Matsumoto             */
  }
}

/* initialize by an array with array-length */
/* init_key is the array for initializing keys */
/* key_length is its length */
/* slight change for C++, 2004/2/26 */
static void init_by_array (ddsrt_prng_t *prng, const uint32_t init_key[], size_t key_length)
{
  uint32_t i, j, k;
  init_genrand (prng, 19650218U);
  i = 1; j = 0;
  k = (N > key_length ? N : (uint32_t) key_length);
  for (; k; k--)
  {
    prng->mt[i] = (prng->mt[i] ^ ((prng->mt[i-1] ^ (prng->mt[i-1] >> 30)) * 1664525U)) + init_key[j] + j; /* non linear */
    i++; j++;
    if (i >= N)
    {
      prng->mt[0] = prng->mt[N-1];
      i=1;
    }
    if (j >= key_length)
    {
      j = 0;
    }
  }
  for (k = N-1; k; k--)
  {
    prng->mt[i] = (prng->mt[i] ^ ((prng->mt[i-1] ^ (prng->mt[i-1] >> 30)) * 1566083941U)) - i; /* non linear */
    i++;
    if (i >= N)
    {
      prng->mt[0] = prng->mt[N-1];
      i = 1;
    }
  }
  prng->mt[0] = 0x80000000U; /* MSB is 1; assuring non-zero initial array */
}

void ddsrt_prng_init_simple (ddsrt_prng_t *prng, uint32_t seed)
{
  init_genrand (prng, seed);
}

void ddsrt_prng_init (ddsrt_prng_t *prng, const struct ddsrt_prng_seed *seed)
{
  init_by_array (prng, seed->key, sizeof (seed->key) / sizeof (seed->key[0]));
}

/* generates a random number on [0,0xffffffff]-interval */
uint32_t ddsrt_prng_random (ddsrt_prng_t *prng)
{
  /* mag01[x] = x * MATRIX_A  for x=0,1 */
  static const uint32_t mag01[2] = { 0x0U, MATRIX_A };
  uint32_t y;

  if (prng->mti >= N)
  {
    /* generate N words at one time */
    int kk;

    for (kk=0; kk < N-M; kk++)
    {
      y = (prng->mt[kk] & UPPER_MASK) | (prng->mt[kk+1] & LOWER_MASK);
      prng->mt[kk] = prng->mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1U];
    }
    for (; kk < N-1; kk++)
    {
      y = (prng->mt[kk] & UPPER_MASK) | (prng->mt[kk+1] & LOWER_MASK);
      prng->mt[kk] = prng->mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1U];
    }
    y = (prng->mt[N-1] & UPPER_MASK) | (prng->mt[0] & LOWER_MASK);
    prng->mt[N-1] = prng->mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1U];

    prng->mti = 0;
  }

  y = prng->mt[prng->mti++];

  /* Tempering */
  y ^= (y >> 11);
  y ^= (y << 7) & 0x9d2c5680U;
  y ^= (y << 15) & 0xefc60000U;
  y ^= (y >> 18);

  return y;
}

uint32_t ddsrt_random (void)
{
  uint32_t x;
  ddsrt_mutex_lock (&default_prng_lock);
  x = ddsrt_prng_random (&default_prng);
  ddsrt_mutex_unlock (&default_prng_lock);
  return x;
}

static const char* pregrams[] = {
  "tre", "tru", "tri", "tro", "tra", "ste", "stu", "sti", "sto", "sta", "sre", "sru",
  "sri", "sro", "sra", "pre", "pru", "pri", "pro", "pra", "ple", "plu", "pli", "plo",
  "pla", "sle", "slu", "sli", "slo", "sla", "kre", "kru", "kri", "kro", "kra", "kle",
  "klu", "kli", "klo", "kla", "kne", "knu", "kni", "kno", "kna", "dre", "dru", "dri",
  "dro", "che", "chu", "chi", "cho", "cha", "fre", "fru", "fri", "fro", "fra", "fle",
  "flu", "fli", "flo", "fla"
};
DDSRT_STATIC_ASSERT(sizeof(pregrams) / sizeof(const char*) == 0x40);

static const char* bigrams[] = {
  "wu", "wi", "wa", "weu", "wau", "woe", "wei", "woi", "re", "ru", "ri", "ro",
  "ra", "rau", "roe", "roi", "te", "tu", "ti", "ta", "tau", "toi", "tai", "pe",
  "pu", "pi", "peu", "pau", "poe", "poi", "pai", "se", "su", "si", "so", "sa",
  "seu", "sau", "soe", "sei", "soi", "sai", "de", "di", "do", "da", "deu", "doe",
  "doi", "dai", "fe", "fi", "fo", "feu", "fau", "foe", "fei", "foi", "ge", "gu",
  "geu", "gau", "goe", "gei", "goi", "gai", "ke", "ki", "ko", "ka", "keu", "kei",
  "koi", "le", "lu", "li", "lo", "la", "leu", "loe", "lai", "zi", "zo", "za",
  "zeu", "zau", "zoe", "zei", "zoi", "zai", "ce", "cu", "ci", "co", "ca", "ceu",
  "coi", "cai", "vi", "vo", "va", "veu", "vei", "vai", "be", "bu", "bi", "bo",
  "ba", "beu", "boe", "bei", "bai", "ne", "nu", "ni", "no", "neu", "nau", "noe",
  "nei", "me", "mu", "mo", "ma", "meu", "mei", "mai"
};
DDSRT_STATIC_ASSERT(sizeof(bigrams) / sizeof(const char*) == 0x80);


static const char *trigrams[] = {
  "wer", "wet", "wes", "wed", "weg", "wel", "wez", "wec", "wev", "web", "wen", "wem",
  "wuw", "wur", "wut", "wup", "wus", "wud", "wug", "wuk", "wuz", "wuv", "wub", "wun",
  "wum", "wiw", "wit", "wid", "wif", "wig", "wik", "wil", "wiz", "wic", "wiv", "win",
  "wim", "wot", "wop", "wos", "wod", "wof", "wol", "woz", "woc", "wov", "wob", "won",
  "wom", "waw", "war", "wat", "was", "wad", "waf", "wag", "wal", "waz", "wac", "wav",
  "wan", "wam", "rer", "ret", "rep", "res", "red", "ref", "rek", "rel", "rez", "rec",
  "reb", "ren", "ruw", "rur", "rut", "rup", "rus", "rud", "ruf", "rug", "ruz", "ruc",
  "run", "riw", "rir", "rit", "rip", "ris", "rid", "rif", "rik", "ril", "riz", "ric",
  "riv", "rib", "rin", "row", "ror", "rop", "ros", "rof", "rok", "rol", "roz", "roc",
  "rov", "rob", "rom", "raw", "rar", "rat", "rap", "ras", "rad", "rag", "rak", "ral",
  "rac", "rav", "ran", "ram", "ter", "tet", "tep", "ted", "tef", "teg", "tel", "tez",
  "tec", "tev", "ten", "tem", "tuw", "tur", "tut", "tup", "tus", "tud", "tuf", "tuk",
  "tul", "tuv", "tun", "tum", "tir", "tip", "tis", "tid", "tif", "tig", "tik", "til",
  "tic", "tiv", "tib", "tin", "tor", "tot", "top", "tos", "tod", "tof", "tog", "tok",
  "tol", "toc", "tov", "tob", "ton", "tom", "taw", "tar", "tap", "tas", "taf", "tag",
  "tak", "taz", "tac", "tav", "tab", "tan", "tam", "pew", "per", "pet", "pep", "pes",
  "ped", "pek", "pel", "pez", "pec", "peb", "pem", "puw", "put", "pus", "pud", "puf",
  "pug", "puk", "puz", "puc", "puv", "pub", "pun", "pum", "piw", "pir", "pid", "pif",
  "pig", "pil", "piz", "pic", "piv", "pib", "pin", "pim", "pow", "por", "pot", "pop",
  "pod", "pof", "pok", "poz", "poc", "pob", "pon", "pom", "paw", "par", "pat", "pap",
  "pas", "pad", "paf", "pag", "pak", "pal", "pac", "pav", "pan", "sew", "set", "ses",
  "sed", "sef", "seg", "sek", "sel", "sez", "sec", "sev", "seb", "sen", "sem", "suw",
  "sur", "sut", "sup", "sud", "suf", "sug", "suk", "sul", "suz", "suc", "suv", "sub",
  "sun", "sum", "siw", "sir", "sit", "sip", "sid", "sig", "sik", "sil", "siz", "sic",
  "sib", "sin", "sow", "sor", "sot", "sop", "sos", "sof", "sog", "sok", "sol", "soc",
  "sob", "son", "saw", "sar", "sat", "sas", "sad", "sag", "sak", "sav", "sab", "san",
  "dew", "der", "det", "dep", "des", "ded", "deg", "del", "dec", "dev", "den", "dem",
  "dur", "dut", "dup", "dus", "dud", "dug", "duk", "dul", "duz", "duc", "duv", "dub",
  "dun", "dum", "diw", "dir", "dit", "dis", "did", "dif", "dig", "dik", "dil", "diz",
  "dic", "div", "dib", "din", "dim", "dow", "dor", "dot", "dop", "dos", "dod", "dok",
  "dol", "dob", "daw", "dar", "dat", "dap", "das", "daf", "dag", "dak", "daz", "dac",
  "dab", "few", "fer", "fet", "fep", "fes", "fed", "fef", "feg", "fek", "fel", "fec",
  "fev", "feb", "fen", "fem", "fuw", "fur", "fup", "fus", "fud", "ful", "fuv", "fub",
  "fun", "fum", "fiw", "fir", "fit", "fip", "fis", "fid", "fif", "fig", "fik", "fil",
  "fiz", "fic", "fiv", "fin", "fim", "fow", "for", "fop", "fod", "fof", "fog", "foz",
  "foc", "fov", "fon", "fom", "faw", "far", "fat", "fap", "fas", "fad", "fal", "faz",
  "fac", "fav", "fab", "fan", "fam", "gew", "ger", "get", "gep", "ges", "ged", "gef",
  "geg", "gek", "gel", "gec", "gev", "geb", "gen", "guw", "gur", "gut", "gup", "gus",
  "gud", "guf", "gug", "gul", "guz", "guc", "gun", "git", "gip", "gis", "gid", "gif",
  "gig", "gik", "gil", "giv", "gib", "gin", "gim", "gor", "got", "gop", "god", "gof",
  "gog", "gok", "gol", "goz", "goc", "gov", "gob", "gon", "gom", "gaw", "gar", "gat",
  "gap", "gas", "gaf", "gag", "gal", "gaz", "gac", "gav", "gab", "gan", "gam", "kew",
  "ker", "ket", "kep", "kes", "ked", "kef", "keg", "kek", "kez", "kec", "kev", "keb",
  "ken", "kem", "kuw", "kur", "kut", "kup", "kus", "kud", "kuf", "kul", "kuc", "kub",
  "kun", "kiw", "kir", "kit", "kip", "kis", "kid", "kif", "kig", "kik", "kil", "kiz",
  "kic", "kib", "kin", "kim", "kor", "kot", "kos", "kod", "kof", "kog", "kol", "koz",
  "kon", "kom", "kaw", "kap", "kas", "kad", "kag", "kak", "kal", "kaz", "kac", "kav",
  "kab", "kan", "kam", "lew", "ler", "let", "lep", "led", "lef", "leg", "lek", "lel",
  "leb", "len", "luw", "lur", "lut", "lup", "lus", "lud", "lug", "luk", "lul", "luz",
  "luc", "luv", "lub", "lun", "liw", "lir", "lip", "lis", "lid", "lif", "lig", "lik",
  "liz", "lic", "liv", "lib", "lin", "lim", "low", "lor", "lot", "lop", "los", "lod",
  "log", "lol", "loz", "loc", "lov", "lob", "lon", "lom", "law", "lar", "lat", "lap",
  "las", "laf", "lak", "lal", "lac", "lav", "lab", "zew", "zer", "zet", "zep", "zes",
  "zef", "zeg", "zek", "zel", "zez", "zec", "zev", "zen", "zem", "zur", "zut", "zup",
  "zus", "zud", "zuf", "zug", "zuk", "zuz", "zuv", "zub", "zun", "zum", "ziw", "zir",
  "zis", "zid", "zif", "zig", "zik", "ziz", "zic", "ziv", "zib", "zin", "zim", "zow",
  "zor", "zot", "zop", "zod", "zof", "zog", "zok", "zol", "zoz", "zoc", "zob", "zon",
  "zom", "zaw", "zar", "zat", "zap", "zas", "zad", "zag", "zal", "zaz", "zac", "zav",
  "zab", "zan", "zam", "cew", "cer", "cet", "cep", "ces", "ced", "cef", "ceg", "cek",
  "cel", "cez", "cec", "cev", "cen", "cem", "cuw", "cur", "cut", "cup", "cus", "cud",
  "cug", "cul", "cuc", "cuv", "cub", "cun", "ciw", "cir", "cit", "cip", "cis", "cid",
  "cif", "cik", "cil", "ciz", "cic", "civ", "cib", "cin", "cim", "cow", "cot", "cop",
  "cod", "cog", "col", "cov", "cob", "con", "com", "caw", "car", "cat", "cas", "cad",
  "caf", "cag", "cal", "caz", "cac", "cav", "cab", "vew", "ver", "vet", "vep", "ves",
  "vef", "veg", "vek", "vel", "vez", "vec", "vev", "ven", "vem", "vuw", "vur", "vut",
  "vup", "vud", "vug", "vuk", "vuz", "vuc", "vuv", "vub", "vum", "vir", "vit", "vip",
  "vis", "vid", "vif", "vig", "vik", "vil", "viz", "viv", "vib", "vin", "vim", "vor",
  "vot", "vop", "vos", "vod", "vog", "vok", "vol", "voz", "voc", "von", "vaw", "var",
  "vat", "vap", "vas", "vad", "vaf", "vag", "vak", "val", "vaz", "vac", "vav", "vab",
  "van", "vam", "bew", "ber", "bet", "bep", "bes", "bed", "bef", "beg", "bek", "bel",
  "bez", "bec", "bev", "beb", "bem", "buw", "bur", "but", "bus", "buf", "buk", "bul",
  "buz", "bub", "bun", "bir", "bit", "bip", "bid", "bif", "bik", "bil", "biz", "bic",
  "biv", "bib", "bin", "bim", "bow", "bor", "bop", "bos", "bod", "bof", "bog", "bok",
  "bol", "boz", "boc", "bov", "bob", "bom", "baw", "bar", "bat", "bap", "bas", "bad",
  "baf", "bag", "bak", "bal", "baz", "bac", "bav", "bab", "bam", "new", "ner", "net",
  "nes", "nef", "neg", "nez", "nev", "neb", "nen", "nuw", "nur", "nut", "nup", "nus",
  "nuf", "nug", "nuk", "nul", "nuz", "nuc", "nuv", "nun", "num", "nir", "nip", "nis",
  "nid", "nif", "nik", "niz", "niv", "nib", "nin", "nim", "now", "nor", "not", "nop",
  "nos", "nod", "nof", "nok", "noz", "nov", "nom", "naw", "nar", "nat", "nap", "nas",
  "nad", "naf", "nag", "nak", "nal", "naz", "nac", "nab", "nan", "nam", "mew", "mer",
  "met", "mep", "mes", "med", "mef", "meg", "mek", "mel", "mez", "mec", "mev", "meb",
  "men", "mem", "mur", "mut", "mus", "muf", "mug", "muk", "mul", "muz", "muc", "muv",
  "mub", "mun", "mum", "miw", "mir", "mit", "mip", "mis", "mid", "mif", "mik", "mil",
  "miz", "mic", "miv", "mib", "mim", "mow", "mor", "mot", "mop", "mod", "mof", "mog",
  "mok", "mol", "moz", "moc", "mov", "mon", "mom", "maw", "mar", "map", "mad", "mag",
  "maz", "mac", "mav", "mab"
};
DDSRT_STATIC_ASSERT(sizeof(trigrams) / sizeof(const char*) == 0x400);



size_t ddsrt_prng_random_name(ddsrt_prng_t *prng, char* output, size_t output_size)
{
  /*
    pregram:  6 bits randomness,   3 char length
    bigram:   7 bits randomness, 2/3 char length
    trigram: 10 bits randomness,   3 char length

    prefix + 2*bigram + trigram:
      30 bits randomness
      10 to 12 char length + \0
  */

  uint32_t rand = ddsrt_prng_random(prng);
  uint32_t pregram_index  = (rand & 0xFC000000) >> 26;
  uint32_t bigram_1_index = (rand & 0x03F80000) >> 19;
  uint32_t bigram_2_index = (rand & 0x0007F000) >> 12;
  uint32_t trigram_index  = (rand & 0x00000FFC) >> 02;

  const char * parts[] = {
    pregrams[pregram_index],
    bigrams[bigram_1_index],
    bigrams[bigram_2_index],
    trigrams[trigram_index]
  };

  size_t slen = 0;
  if (output_size > 0) {
    for (size_t i = 0; i < 4; ++i) {
      size_t len = strlen(parts[i]);
      if (len > output_size - 1) len = output_size - 1;
      slen += len;
      memcpy(output, parts[i], len);

      if (i == 0 && len > 0) {
        // capitalize
        *output = (char) toupper((char) *output);
      }

      output += len;
      output_size -= len;
    }
    // The above loop always leaves at least one character space for the zero terminator
    *output = '\0';
  }
  return slen;
}

void ddsrt_random_init (void)
{
  ddsrt_prng_seed_t seed;
  if (!ddsrt_prng_makeseed (&seed))
  {
    static ddsrt_atomic_uint32_t counter = DDSRT_ATOMIC_UINT32_INIT (0);
    /* Poor man's initialisation */
    DDSRT_STATIC_ASSERT (sizeof (seed.key) / sizeof (seed.key[0]) >= 4);
    memset (&seed, 0, sizeof (seed));
    dds_time_t now = dds_time ();
    seed.key[0] = (uint32_t) ddsrt_getpid ();
    seed.key[1] = (uint32_t) ((uint64_t) now >> 32);
    seed.key[2] = (uint32_t) now;
    seed.key[3] = ddsrt_atomic_inc32_ov (&counter);
  }
  ddsrt_prng_init (&default_prng, &seed);
  ddsrt_mutex_init (&default_prng_lock);
}

void ddsrt_random_fini (void)
{
  ddsrt_mutex_destroy (&default_prng_lock);
}
