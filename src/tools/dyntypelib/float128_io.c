/*
 * Copyright(c) 2026 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#define _CRT_SECURE_NO_WARNINGS

#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/static_assert.h"

#include "float128_io.h"

#define F128_EXP_BIAS 16383
#define F128_EXP_INF_NAN 0x7fff
#define F128_FRAC_BITS 112
#define F128_PRECISION 113
#define F128_DECIMAL_DIGITS 36
/* These caps exceed the longest terminating decimal/hex spelling needed for
   any binary128 rounding boundary, so the rest can be represented as sticky. */
#define F128_DECIMAL_PARSE_DIGITS 13000
#define F128_HEX_PARSE_DIGITS 5000

#ifndef FLT_DECIMAL_DIG
#define FLT_DECIMAL_DIG 9
#endif

#ifndef DBL_DECIMAL_DIG
#define DBL_DECIMAL_DIG 17
#endif

DDSRT_STATIC_ASSERT (FLT_RADIX == 2);
DDSRT_STATIC_ASSERT (sizeof (float) == sizeof (uint32_t));
DDSRT_STATIC_ASSERT (FLT_MANT_DIG == 24);
DDSRT_STATIC_ASSERT (FLT_MAX_EXP == 128);
DDSRT_STATIC_ASSERT (FLT_MIN_EXP == -125);
DDSRT_STATIC_ASSERT (sizeof (double) == sizeof (uint64_t));
DDSRT_STATIC_ASSERT (DBL_MANT_DIG == 53);
DDSRT_STATIC_ASSERT (DBL_MAX_EXP == 1024);
DDSRT_STATIC_ASSERT (DBL_MIN_EXP == -1021);
DDSRT_STATIC_ASSERT (sizeof (dtl_float128_t) == 16);

struct f128_bits {
  uint64_t hi;
  uint64_t lo;
};

struct bigint {
  uint32_t *v;
  size_t n;
  size_t cap;
};

static void bigint_init (struct bigint *a)
{
  a->v = NULL;
  a->n = 0;
  a->cap = 0;
}

static void bigint_fini (struct bigint *a)
{
  ddsrt_free (a->v);
}

static void bigint_normalize (struct bigint *a)
{
  while (a->n > 0 && a->v[a->n - 1] == 0)
    a->n--;
}

static bool bigint_reserve (struct bigint *a, size_t cap)
{
  if (cap <= a->cap)
    return true;
  size_t ncap = a->cap ? a->cap : 4;
  while (ncap < cap)
  {
    if (ncap > SIZE_MAX / 2)
    {
      ncap = cap;
      break;
    }
    ncap *= 2;
  }
  uint32_t *v = ddsrt_realloc (a->v, ncap * sizeof (*v));
  if (v == NULL)
    return false;
  a->v = v;
  a->cap = ncap;
  return true;
}

static bool bigint_set_u32 (struct bigint *a, uint32_t v)
{
  if (v == 0)
  {
    a->n = 0;
    return true;
  }
  if (!bigint_reserve (a, 1))
    return false;
  a->v[0] = v;
  a->n = 1;
  return true;
}

static bool bigint_set_u64 (struct bigint *a, uint64_t v)
{
  if (v == 0)
  {
    a->n = 0;
    return true;
  }
  if (!bigint_reserve (a, 2))
    return false;
  a->v[0] = (uint32_t) v;
  a->v[1] = (uint32_t) (v >> 32);
  a->n = a->v[1] ? 2 : 1;
  return true;
}

static bool bigint_copy (struct bigint *dst, const struct bigint *src)
{
  if (!bigint_reserve (dst, src->n))
    return false;
  if (src->n != 0)
    memcpy (dst->v, src->v, src->n * sizeof (*src->v));
  dst->n = src->n;
  return true;
}

static bool bigint_is_zero (const struct bigint *a)
{
  return a->n == 0;
}

static int bigint_cmp (const struct bigint *a, const struct bigint *b)
{
  if (a->n != b->n)
    return (a->n > b->n) ? 1 : -1;
  for (size_t i = a->n; i > 0; i--)
  {
    if (a->v[i - 1] != b->v[i - 1])
      return (a->v[i - 1] > b->v[i - 1]) ? 1 : -1;
  }
  return 0;
}

static bool bigint_add_small (struct bigint *a, uint32_t b)
{
  if (b == 0)
    return true;
  if (a->n == 0)
    return bigint_set_u32 (a, b);
  uint64_t carry = b;
  size_t i = 0;
  while (carry != 0 && i < a->n)
  {
    carry += a->v[i];
    a->v[i] = (uint32_t) carry;
    carry >>= 32;
    i++;
  }
  if (carry != 0)
  {
    if (!bigint_reserve (a, a->n + 1))
      return false;
    a->v[a->n++] = (uint32_t) carry;
  }
  return true;
}

static bool bigint_sub_small (struct bigint *a, uint32_t b)
{
  if (b == 0)
    return true;
  uint64_t borrow = b;
  for (size_t i = 0; borrow != 0 && i < a->n; i++)
  {
    const uint64_t av = a->v[i];
    a->v[i] = (uint32_t) (av - borrow);
    borrow = (av < borrow);
  }
  if (borrow != 0)
    return false;
  bigint_normalize (a);
  return true;
}

static bool bigint_mul_small (struct bigint *a, uint32_t b)
{
  if (a->n == 0 || b == 1)
    return true;
  if (b == 0)
  {
    a->n = 0;
    return true;
  }
  uint64_t carry = 0;
  for (size_t i = 0; i < a->n; i++)
  {
    const uint64_t x = (uint64_t) a->v[i] * b + carry;
    a->v[i] = (uint32_t) x;
    carry = x >> 32;
  }
  if (carry != 0)
  {
    if (!bigint_reserve (a, a->n + 1))
      return false;
    a->v[a->n++] = (uint32_t) carry;
  }
  return true;
}

static bool bigint_mul_pow5 (struct bigint *a, uint64_t n)
{
  for (uint64_t i = 0; i < n; i++)
    if (!bigint_mul_small (a, 5))
      return false;
  return true;
}

static bool bigint_shift_left_bits (struct bigint *a, uint64_t bits)
{
  if (a->n == 0 || bits == 0)
    return true;
  const uint64_t limb_shift64 = bits / 32;
  const unsigned bit_shift = (unsigned) (bits % 32);
  if (limb_shift64 > SIZE_MAX - a->n - 1)
    return false;
  const size_t limb_shift = (size_t) limb_shift64;
  const size_t oldn = a->n;
  const size_t newcap = oldn + limb_shift + 1;
  if (!bigint_reserve (a, newcap))
    return false;
  if (limb_shift != 0)
  {
    for (size_t i = oldn; i > 0; i--)
      a->v[i - 1 + limb_shift] = a->v[i - 1];
    memset (a->v, 0, limb_shift * sizeof (*a->v));
    a->n = oldn + limb_shift;
  }
  if (bit_shift != 0)
  {
    uint32_t carry = 0;
    for (size_t i = 0; i < a->n; i++)
    {
      const uint32_t limb = a->v[i];
      a->v[i] = (limb << bit_shift) | carry;
      carry = limb >> (32 - bit_shift);
    }
    if (carry != 0)
      a->v[a->n++] = carry;
  }
  return true;
}

static bool bigint_shift_right1 (struct bigint *a)
{
  uint32_t carry = 0;
  for (size_t i = a->n; i > 0; i--)
  {
    const uint32_t limb = a->v[i - 1];
    a->v[i - 1] = (limb >> 1) | (carry << 31);
    carry = limb & 1u;
  }
  bigint_normalize (a);
  return true;
}

static bool bigint_set_bit (struct bigint *a, size_t bit)
{
  const size_t limb = bit / 32;
  if (!bigint_reserve (a, limb + 1))
    return false;
  while (a->n <= limb)
    a->v[a->n++] = 0;
  a->v[limb] |= (uint32_t) 1u << (bit % 32);
  return true;
}

static bool bigint_get_bit (const struct bigint *a, size_t bit)
{
  const size_t limb = bit / 32;
  return limb < a->n && ((a->v[limb] >> (bit % 32)) & 1u) != 0;
}

static size_t bigint_bitlen (const struct bigint *a)
{
  if (a->n == 0)
    return 0;
  uint32_t top = a->v[a->n - 1];
  size_t bits = (a->n - 1) * 32;
  while (top != 0)
  {
    bits++;
    top >>= 1;
  }
  return bits;
}

static void bigint_sub (struct bigint *a, const struct bigint *b)
{
  uint64_t borrow = 0;
  assert (bigint_cmp (a, b) >= 0);
  for (size_t i = 0; i < a->n; i++)
  {
    const uint64_t av = a->v[i];
    const uint64_t bv = (i < b->n) ? b->v[i] : 0;
    const uint64_t r = av - bv - borrow;
    a->v[i] = (uint32_t) r;
    borrow = (av < bv + borrow);
  }
  assert (borrow == 0);
  bigint_normalize (a);
}

static bool bigint_divmod (const struct bigint *num, const struct bigint *den, struct bigint *q, struct bigint *rem)
{
  assert (!bigint_is_zero (den));
  if (!bigint_set_u32 (q, 0) || !bigint_set_u32 (rem, 0))
    return false;
  const size_t nbits = bigint_bitlen (num);
  for (size_t i = nbits; i > 0; i--)
  {
    if (!bigint_shift_left_bits (rem, 1))
      return false;
    if (bigint_get_bit (num, i - 1) && !bigint_add_small (rem, 1))
      return false;
    if (bigint_cmp (rem, den) >= 0)
    {
      bigint_sub (rem, den);
      if (!bigint_set_bit (q, i - 1))
        return false;
    }
  }
  bigint_normalize (q);
  return true;
}

static uint32_t bigint_div_small (struct bigint *a, uint32_t d)
{
  uint64_t rem = 0;
  assert (d != 0);
  for (size_t i = a->n; i > 0; i--)
  {
    const uint64_t cur = (rem << 32) | a->v[i - 1];
    a->v[i - 1] = (uint32_t) (cur / d);
    rem = cur % d;
  }
  bigint_normalize (a);
  return (uint32_t) rem;
}

static bool bigint_to_decimal (const struct bigint *a, char *buf, size_t bufsz)
{
  struct bigint tmp;
  bigint_init (&tmp);
  bool ok = false;
  if (!bigint_copy (&tmp, a))
    goto done;
  if (bigint_is_zero (&tmp))
  {
    if (bufsz < 2)
      goto done;
    strcpy (buf, "0");
    ok = true;
    goto done;
  }
  char rbuf[128];
  size_t n = 0;
  while (!bigint_is_zero (&tmp))
  {
    if (n == sizeof (rbuf) - 1)
      goto done;
    rbuf[n++] = (char) ('0' + bigint_div_small (&tmp, 10));
  }
  if (n + 1 > bufsz)
    goto done;
  for (size_t i = 0; i < n; i++)
    buf[i] = rbuf[n - 1 - i];
  buf[n] = 0;
  ok = true;
done:
  bigint_fini (&tmp);
  return ok;
}

static uint64_t bigint_bits64 (const struct bigint *a, size_t first_bit)
{
  uint64_t x = 0;
  for (size_t i = 0; i < 64; i++)
    if (bigint_get_bit (a, first_bit + i))
      x |= (uint64_t) 1 << i;
  return x;
}

static bool bigint_from_significand (struct bigint *m, uint64_t frac_hi, uint64_t frac_lo, bool implicit)
{
  if (!bigint_set_u64 (m, frac_lo))
    return false;
  if (frac_hi != 0)
  {
    if (!bigint_set_bit (m, 64))
      return false;
    m->v[2] = (uint32_t) frac_hi;
    if ((frac_hi >> 32) != 0)
    {
      if (!bigint_set_bit (m, 96))
        return false;
      m->v[3] = (m->v[3] & 0xffff0000u) | (uint32_t) (frac_hi >> 32);
    }
    bigint_normalize (m);
  }
  if (implicit && !bigint_set_bit (m, F128_FRAC_BITS))
    return false;
  return true;
}

static struct f128_bits f128_load_native (const dtl_float128_t *p)
{
  ddsrt_uint128_t u;
  memcpy (&u, p->x, sizeof (u));
  return (struct f128_bits) { .hi = u.h, .lo = u.l };
}

static void f128_store_native (dtl_float128_t *p, struct f128_bits b)
{
  const ddsrt_uint128_t u = { .h = b.hi, .l = b.lo };
  memcpy (p->x, &u, sizeof (u));
}

static void f128_store_zero (dtl_float128_t *out, bool sign)
{
  f128_store_native (out, (struct f128_bits) { .hi = sign ? UINT64_C (1) << 63 : 0, .lo = 0 });
}

static void f128_store_inf (dtl_float128_t *out, bool sign)
{
  f128_store_native (out, (struct f128_bits) {
    .hi = (sign ? UINT64_C (1) << 63 : 0) | ((uint64_t) F128_EXP_INF_NAN << 48),
    .lo = 0
  });
}

static void f128_store_nan (dtl_float128_t *out, bool sign)
{
  f128_store_native (out, (struct f128_bits) {
    .hi = (sign ? UINT64_C (1) << 63 : 0) | ((uint64_t) F128_EXP_INF_NAN << 48) | (UINT64_C (1) << 47),
    .lo = 0
  });
}

static unsigned floor_log2_u64 (uint64_t v)
{
  unsigned n = 0;
  assert (v != 0);
  while ((v >>= 1) != 0)
    n++;
  return n;
}

static void f128_store_from_frac (dtl_float128_t *out, bool sign, uint16_t exp, uint64_t frac, unsigned frac_bits)
{
  const unsigned shift = F128_FRAC_BITS - frac_bits;
  uint64_t frac_hi;
  uint64_t frac_lo;
  if (shift >= 64)
  {
    frac_hi = frac << (shift - 64);
    frac_lo = 0;
  }
  else if (shift == 0)
  {
    frac_hi = 0;
    frac_lo = frac;
  }
  else
  {
    frac_hi = frac >> (64 - shift);
    frac_lo = frac << shift;
  }
  f128_store_native (out, (struct f128_bits) {
    .hi = (sign ? UINT64_C (1) << 63 : 0) | ((uint64_t) exp << 48) |
        (frac_hi & UINT64_C (0x0000ffffffffffff)),
    .lo = frac_lo
  });
}

static void f128_store_nan_payload (dtl_float128_t *out, bool sign, uint64_t frac, unsigned frac_bits)
{
  const uint64_t quiet_frac = frac | (UINT64_C (1) << (frac_bits - 1));
  f128_store_from_frac (out, sign, F128_EXP_INF_NAN, quiet_frac, frac_bits);
}

static void f128_from_binary (dtl_float128_t *out, bool sign, uint64_t exp, uint64_t frac,
    unsigned frac_bits, unsigned exp_inf_nan, int exp_bias)
{
  if (exp == exp_inf_nan)
  {
    if (frac == 0)
      f128_store_inf (out, sign);
    else
      f128_store_nan_payload (out, sign, frac, frac_bits);
  }
  else if (exp == 0)
  {
    if (frac == 0)
      f128_store_zero (out, sign);
    else
    {
      const unsigned k = floor_log2_u64 (frac);
      const uint64_t leading = UINT64_C (1) << k;
      const int e2 = 1 - exp_bias - (int) frac_bits + (int) k;
      f128_store_from_frac (out, sign, (uint16_t) (e2 + F128_EXP_BIAS), frac - leading, k);
    }
  }
  else
  {
    f128_store_from_frac (out, sign, (uint16_t) ((int) exp - exp_bias + F128_EXP_BIAS), frac, frac_bits);
  }
}

void dtl_float128_from_float32 (dtl_float128_t *out, float v)
{
  uint32_t bits;
  memcpy (&bits, &v, sizeof (bits));
  f128_from_binary (out, (bits >> 31) != 0, (bits >> 23) & 0xffu, bits & UINT32_C (0x7fffff),
      23, 0xffu, 127);
}

void dtl_float128_from_float64 (dtl_float128_t *out, double v)
{
  uint64_t bits;
  memcpy (&bits, &v, sizeof (bits));
  f128_from_binary (out, (bits >> 63) != 0, (bits >> 52) & 0x7ffu, bits & UINT64_C (0x000fffffffffffff),
      52, 0x7ffu, 1023);
}

static int ascii_tolower (char c)
{
  return tolower ((unsigned char) c);
}

static bool ascii_streq (const char *a, size_t n, const char *b)
{
  for (size_t i = 0; i < n || b[i] != 0; i++)
    if (i == n || b[i] == 0 || ascii_tolower (a[i]) != b[i])
      return false;
  return true;
}

static const char *skip_space (const char *s)
{
  while (isspace ((unsigned char) *s))
    s++;
  return s;
}

static bool end_after_space (const char *s)
{
  return *skip_space (s) == 0;
}

static int64_t sat_add_i64 (int64_t a, int64_t b)
{
  if (b > 0 && a > INT64_MAX - b)
    return INT64_MAX;
  if (b < 0 && a < INT64_MIN - b)
    return INT64_MIN;
  return a + b;
}

static int64_t sat_sub_i64 (int64_t a, int64_t b)
{
  if (b == INT64_MIN)
    return (a >= 0) ? INT64_MAX : sat_add_i64 (a, INT64_MAX);
  return sat_add_i64 (a, -b);
}

static int64_t sat_from_size (size_t n)
{
  return n > (size_t) INT64_MAX ? INT64_MAX : (int64_t) n;
}

static int64_t sat_from_u64 (uint64_t n)
{
  return n > (uint64_t) INT64_MAX ? INT64_MAX : (int64_t) n;
}

static int64_t parse_signed_exp (const char **ps, bool *ok)
{
  const char *s = *ps;
  bool neg = false;
  int64_t v = 0;
  *ok = false;
  if (*s == '+' || *s == '-')
    neg = (*s++ == '-');
  if (!isdigit ((unsigned char) *s))
    return 0;
  *ok = true;
  while (isdigit ((unsigned char) *s))
  {
    const int d = *s++ - '0';
    if (v <= (INT64_MAX - d) / 10)
      v = 10 * v + d;
    else
      v = INT64_MAX;
  }
  *ps = s;
  return neg ? -v : v;
}

static bool cmp_scaled (const struct bigint *a, int64_t ashift, const struct bigint *b, int64_t bshift, int *cmp)
{
  struct bigint aa, bb;
  bigint_init (&aa);
  bigint_init (&bb);
  bool ok = false;
  if (ashift < 0 || bshift < 0)
  {
    const int64_t add = ashift < bshift ? -ashift : -bshift;
    ashift += add;
    bshift += add;
  }
  if (!bigint_copy (&aa, a) || !bigint_copy (&bb, b))
    goto done;
  if (!bigint_shift_left_bits (&aa, (uint64_t) ashift) || !bigint_shift_left_bits (&bb, (uint64_t) bshift))
    goto done;
  *cmp = bigint_cmp (&aa, &bb);
  ok = true;
done:
  bigint_fini (&aa);
  bigint_fini (&bb);
  return ok;
}

static bool floor_log2_rational (const struct bigint *a, const struct bigint *b, int64_t shift2, int64_t *e)
{
  const int64_t la = sat_from_size (bigint_bitlen (a));
  const int64_t lb = sat_from_size (bigint_bitlen (b));
  const int64_t q = sat_sub_i64 (la, lb);
  int cmp;
  if (!cmp_scaled (a, 0, b, q, &cmp))
    return false;
  *e = sat_add_i64 (sat_add_i64 (shift2, q), cmp >= 0 ? 0 : -1);
  return true;
}

static bool round_scaled (const struct bigint *a, const struct bigint *b, int64_t shift2, struct bigint *q)
{
  struct bigint num, den, rem, twice_rem;
  bigint_init (&num);
  bigint_init (&den);
  bigint_init (&rem);
  bigint_init (&twice_rem);
  bool ok = false;
  if (!bigint_copy (&num, a) || !bigint_copy (&den, b))
    goto done;
  if (shift2 >= 0)
  {
    if (!bigint_shift_left_bits (&num, (uint64_t) shift2))
      goto done;
  }
  else
  {
    if (!bigint_shift_left_bits (&den, (uint64_t) -shift2))
      goto done;
  }
  if (!bigint_divmod (&num, &den, q, &rem))
    goto done;
  if (!bigint_copy (&twice_rem, &rem) || !bigint_shift_left_bits (&twice_rem, 1))
    goto done;
  const int cmp = bigint_cmp (&twice_rem, &den);
  if (cmp > 0 || (cmp == 0 && bigint_get_bit (q, 0)))
  {
    if (!bigint_add_small (q, 1))
      goto done;
  }
  ok = true;
done:
  bigint_fini (&num);
  bigint_fini (&den);
  bigint_fini (&rem);
  bigint_fini (&twice_rem);
  return ok;
}

static void encode_from_sig (dtl_float128_t *out, bool sign, uint16_t exp, const struct bigint *sig)
{
  const uint64_t frac_lo = bigint_bits64 (sig, 0);
  const uint64_t frac_hi = bigint_bits64 (sig, 64) & UINT64_C (0x0000ffffffffffff);
  f128_store_native (out, (struct f128_bits) {
    .hi = (sign ? UINT64_C (1) << 63 : 0) | ((uint64_t) exp << 48) | frac_hi,
    .lo = frac_lo
  });
}

static bool encode_rational (bool sign, const struct bigint *a, const struct bigint *b, int64_t shift2, dtl_float128_t *out)
{
  int64_t e2;
  struct bigint sig;
  bigint_init (&sig);
  bool ok = false;

  if (bigint_is_zero (a))
  {
    f128_store_zero (out, sign);
    ok = true;
    goto done;
  }
  if (!floor_log2_rational (a, b, shift2, &e2))
    goto done;
  if (e2 > 16383)
  {
    f128_store_inf (out, sign);
    ok = true;
    goto done;
  }
  if (e2 < -16495)
  {
    f128_store_zero (out, sign);
    ok = true;
    goto done;
  }

  if (e2 < -16382)
  {
    if (!round_scaled (a, b, sat_add_i64 (shift2, 16494), &sig))
      goto done;
    if (bigint_is_zero (&sig))
      f128_store_zero (out, sign);
    else if (bigint_bitlen (&sig) > F128_FRAC_BITS)
      f128_store_native (out, (struct f128_bits) {
        .hi = (sign ? UINT64_C (1) << 63 : 0) | (UINT64_C (1) << 48),
        .lo = 0
      });
    else
      encode_from_sig (out, sign, 0, &sig);
  }
  else
  {
    if (!round_scaled (a, b, sat_add_i64 (shift2, F128_FRAC_BITS - e2), &sig))
      goto done;
    if (bigint_bitlen (&sig) > F128_PRECISION)
    {
      if (!bigint_shift_right1 (&sig))
        goto done;
      e2++;
    }
    if (e2 > 16383)
      f128_store_inf (out, sign);
    else
    {
      assert (bigint_get_bit (&sig, F128_FRAC_BITS));
      sig.v[F128_FRAC_BITS / 32] &= ~((uint32_t) 1u << (F128_FRAC_BITS % 32));
      bigint_normalize (&sig);
      encode_from_sig (out, sign, (uint16_t) (e2 + F128_EXP_BIAS), &sig);
    }
  }
  ok = true;
done:
  bigint_fini (&sig);
  return ok;
}

static int hex_value (char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static bool parse_hex (const char *s, bool sign, dtl_float128_t *out)
{
  struct bigint coeff, one;
  bigint_init (&coeff);
  bigint_init (&one);
  bool ok = false, any_digit = false, after_dot = false, exp_ok;
  uint64_t frac_hex = 0;
  uint64_t coeff_hex = 0;
  uint64_t retained_hex = 0;
  bool discarded_nonzero = false;
  if (!bigint_set_u32 (&coeff, 0) || !bigint_set_u32 (&one, 1))
    goto done;
  if (s[0] != '0' || ascii_tolower (s[1]) != 'x')
    goto done;
  s += 2;
  while (*s != 0)
  {
    const int h = hex_value (*s);
    if (h >= 0)
    {
      any_digit = true;
      if (after_dot && frac_hex != UINT64_MAX)
        frac_hex++;
      if (h != 0 || coeff_hex != 0)
      {
        if (coeff_hex != UINT64_MAX)
          coeff_hex++;
        if (retained_hex < F128_HEX_PARSE_DIGITS)
        {
          retained_hex++;
          if (!bigint_mul_small (&coeff, 16) || !bigint_add_small (&coeff, (uint32_t) h))
            goto done;
        }
        else if (h != 0)
        {
          discarded_nonzero = true;
        }
      }
      s++;
    }
    else if (*s == '.' && !after_dot)
    {
      after_dot = true;
      s++;
    }
    else
    {
      break;
    }
  }
  if (!any_digit || ascii_tolower (*s++) != 'p')
    goto done;
  const int64_t exp2 = parse_signed_exp (&s, &exp_ok);
  if (!exp_ok || !end_after_space (s))
    goto done;
  if (bigint_is_zero (&coeff))
  {
    f128_store_zero (out, sign);
    ok = true;
    goto done;
  }
  uint64_t discarded_hex = coeff_hex - retained_hex;
  if (discarded_nonzero)
  {
    if (!bigint_mul_small (&coeff, 16) || !bigint_add_small (&coeff, 1))
      goto done;
    retained_hex++;
    discarded_hex--;
  }
  const int64_t frac_bits = frac_hex > (uint64_t) INT64_MAX / 4 ? INT64_MAX : (int64_t) (4 * frac_hex);
  const int64_t discarded_bits = discarded_hex > (uint64_t) INT64_MAX / 4 ? INT64_MAX : (int64_t) (4 * discarded_hex);
  ok = encode_rational (sign, &coeff, &one, sat_add_i64 (sat_sub_i64 (exp2, frac_bits), discarded_bits), out);
done:
  bigint_fini (&coeff);
  bigint_fini (&one);
  return ok;
}

static bool parse_decimal (const char *s, bool sign, dtl_float128_t *out)
{
  struct bigint coeff, a, b;
  bigint_init (&coeff);
  bigint_init (&a);
  bigint_init (&b);
  bool ok = false, any_digit = false, after_dot = false, exp_ok = true;
  uint64_t frac_digits = 0;
  uint64_t coeff_digits = 0;
  uint64_t retained_digits = 0;
  bool discarded_nonzero = false;
  int64_t exp10 = 0;

  if (!bigint_set_u32 (&coeff, 0))
    goto done;
  while (*s != 0)
  {
    if (isdigit ((unsigned char) *s))
    {
      const int d = *s++ - '0';
      any_digit = true;
      if (after_dot && frac_digits != UINT64_MAX)
        frac_digits++;
      if (d != 0 || !bigint_is_zero (&coeff))
      {
        if (coeff_digits != UINT64_MAX)
          coeff_digits++;
        if (retained_digits < F128_DECIMAL_PARSE_DIGITS)
        {
          retained_digits++;
          if (!bigint_mul_small (&coeff, 10) || !bigint_add_small (&coeff, (uint32_t) d))
            goto done;
        }
        else if (d != 0)
        {
          discarded_nonzero = true;
        }
      }
    }
    else if (*s == '.' && !after_dot)
    {
      after_dot = true;
      s++;
    }
    else
    {
      break;
    }
  }
  if (!any_digit)
    goto done;
  if (*s == 'e' || *s == 'E')
  {
    s++;
    exp10 = parse_signed_exp (&s, &exp_ok);
  }
  if (!exp_ok || !end_after_space (s))
    goto done;
  if (bigint_is_zero (&coeff))
  {
    f128_store_zero (out, sign);
    ok = true;
    goto done;
  }

  uint64_t discarded_digits = coeff_digits - retained_digits;
  if (discarded_nonzero)
  {
    if (!bigint_mul_small (&coeff, 10) || !bigint_add_small (&coeff, 1))
      goto done;
    retained_digits++;
    discarded_digits--;
  }

  exp10 = sat_sub_i64 (exp10, sat_from_u64 (frac_digits));
  const int64_t dec_floor = sat_add_i64 (sat_sub_i64 (sat_from_u64 (coeff_digits), 1), exp10);
  if (dec_floor > 4932)
  {
    f128_store_inf (out, sign);
    ok = true;
    goto done;
  }
  if (dec_floor < -4966)
  {
    f128_store_zero (out, sign);
    ok = true;
    goto done;
  }
  exp10 = sat_add_i64 (exp10, sat_from_u64 (discarded_digits));

  if (exp10 >= 0)
  {
    if (!bigint_copy (&a, &coeff) || !bigint_mul_pow5 (&a, (uint64_t) exp10) || !bigint_set_u32 (&b, 1))
      goto done;
  }
  else
  {
    if (!bigint_copy (&a, &coeff) || !bigint_set_u32 (&b, 1) || !bigint_mul_pow5 (&b, (uint64_t) -exp10))
      goto done;
  }
  ok = encode_rational (sign, &a, &b, exp10, out);
done:
  bigint_fini (&coeff);
  bigint_fini (&a);
  bigint_fini (&b);
  return ok;
}

bool dtl_float128_from_string (const char *str, dtl_float128_t *out)
{
  const char *s = skip_space (str);
  bool sign = false;
  if (*s == '+' || *s == '-')
    sign = (*s++ == '-');

  const char *e = s;
  while (*e != 0 && !isspace ((unsigned char) *e))
    e++;
  const size_t n = (size_t) (e - s);
  if (ascii_streq (s, n, "nan") && end_after_space (e))
  {
    f128_store_nan (out, sign);
    return true;
  }
  if ((ascii_streq (s, n, "inf") || ascii_streq (s, n, "infinity")) && end_after_space (e))
  {
    f128_store_inf (out, sign);
    return true;
  }
  if (s[0] == '0' && ascii_tolower (s[1]) == 'x')
    return parse_hex (s, sign, out);
  return parse_decimal (s, sign, out);
}

static bool make_pow5 (struct bigint *p, uint64_t n)
{
  return bigint_set_u32 (p, 1) && bigint_mul_pow5 (p, n);
}

static bool compare_value_pow10 (const struct bigint *m, int64_t e, int64_t d, int *cmp)
{
  struct bigint p, one, mm;
  bigint_init (&p);
  bigint_init (&one);
  bigint_init (&mm);
  bool ok = false;
  if (!bigint_set_u32 (&one, 1))
    goto done;
  if (d >= 0)
  {
    if (!make_pow5 (&p, (uint64_t) d))
      goto done;
    ok = cmp_scaled (m, e, &p, d, cmp);
  }
  else
  {
    if (!bigint_copy (&mm, m) || !bigint_mul_pow5 (&mm, (uint64_t) -d))
      goto done;
    ok = cmp_scaled (&mm, sat_sub_i64 (e, d), &one, 0, cmp);
  }
done:
  bigint_fini (&p);
  bigint_fini (&one);
  bigint_fini (&mm);
  return ok;
}

static int64_t floor_div_i64 (int64_t a, int64_t b)
{
  assert (b > 0);
  if (a >= 0)
    return a / b;
  return -((int64_t) (((uint64_t) -a + (uint64_t) b - 1) / (uint64_t) b));
}

static bool decimal_exponent (const struct bigint *m, int64_t e, int64_t *d)
{
  int64_t e2 = sat_add_i64 ((int64_t) bigint_bitlen (m) - 1, e);
  int cmp;
  *d = floor_div_i64 (e2 * 78913, 262144);
  while (true)
  {
    if (!compare_value_pow10 (m, e, *d, &cmp))
      return false;
    if (cmp >= 0)
      break;
    (*d)--;
  }
  while (true)
  {
    if (!compare_value_pow10 (m, e, sat_add_i64 (*d, 1), &cmp))
      return false;
    if (cmp < 0)
      break;
    (*d)++;
  }
  return true;
}

static bool rounded_decimal_digits_n (const struct bigint *m, int64_t e, int64_t d, uint32_t ndigits, struct bigint *digits)
{
  struct bigint a, b;
  bigint_init (&a);
  bigint_init (&b);
  bool ok = false;
  const int64_t scale = sat_sub_i64 (d, (int64_t) ndigits - 1);
  if (scale >= 0)
  {
    if (!bigint_copy (&a, m) || !make_pow5 (&b, (uint64_t) scale))
      goto done;
    ok = round_scaled (&a, &b, sat_sub_i64 (e, scale), digits);
  }
  else
  {
    if (!bigint_copy (&a, m) || !bigint_mul_pow5 (&a, (uint64_t) -scale) || !bigint_set_u32 (&b, 1))
      goto done;
    ok = round_scaled (&a, &b, sat_sub_i64 (e, scale), digits);
  }
done:
  bigint_fini (&a);
  bigint_fini (&b);
  return ok;
}

static bool write_candidate_string (char *buf, size_t bufsz, const char *s)
{
  const size_t n = strlen (s);
  if (n + 1 > bufsz)
    return false;
  memcpy (buf, s, n + 1);
  return true;
}

static bool write_candidate_parts (char *buf, size_t bufsz, bool sign, const char *digits, int64_t scale)
{
  char fixed[DTL_FLOAT128_STRING_BUFSZ];
  char expform[DTL_FLOAT128_STRING_BUFSZ];
  const size_t ndig = strlen (digits);
  const size_t signlen = sign ? 1 : 0;
  bool have_fixed = false;
  bool have_exp = false;
  int n;

  if (scale == 0)
  {
    n = snprintf (fixed, sizeof (fixed), "%s%s", sign ? "-" : "", digits);
    have_fixed = n >= 0 && (size_t) n < sizeof (fixed);
  }
  else if (scale > 0)
  {
    if ((uint64_t) scale <= sizeof (fixed) - signlen - ndig - 1)
    {
      char *p = fixed;
      if (sign)
        *p++ = '-';
      memcpy (p, digits, ndig);
      p += ndig;
      memset (p, '0', (size_t) scale);
      p += (size_t) scale;
      *p = 0;
      have_fixed = true;
    }
  }
  else
  {
    const int64_t point_pos = (int64_t) ndig + scale;
    if (point_pos > 0)
    {
      n = snprintf (fixed, sizeof (fixed), "%s%.*s.%s",
          sign ? "-" : "", (int) point_pos, digits, digits + point_pos);
      have_fixed = n >= 0 && (size_t) n < sizeof (fixed);
    }
    else if ((uint64_t) -point_pos <= sizeof (fixed) - signlen - ndig - 3)
    {
      char *p = fixed;
      if (sign)
        *p++ = '-';
      *p++ = '0';
      *p++ = '.';
      memset (p, '0', (size_t) -point_pos);
      p += (size_t) -point_pos;
      memcpy (p, digits, ndig);
      p += ndig;
      *p = 0;
      have_fixed = true;
    }
  }

  if (scale == 0)
    have_exp = write_candidate_string (expform, sizeof (expform), fixed);
  else
  {
    n = snprintf (expform, sizeof (expform), "%s%se%"PRId64, sign ? "-" : "", digits, scale);
    have_exp = n >= 0 && (size_t) n < sizeof (expform);
  }

  if (have_fixed && have_exp)
  {
    const char *best = strlen (fixed) <= strlen (expform) ? fixed : expform;
    return write_candidate_string (buf, bufsz, best);
  }
  if (have_fixed)
    return write_candidate_string (buf, bufsz, fixed);
  if (have_exp)
    return write_candidate_string (buf, bufsz, expform);
  return false;
}

static bool format_decimal_candidate (char *buf, size_t bufsz, bool sign, const struct bigint *coeff, int64_t scale)
{
  char digits[128];
  if (bigint_is_zero (coeff) || !bigint_to_decimal (coeff, digits, sizeof (digits)))
    return false;
  size_t ndig = strlen (digits);
  while (ndig > 1 && digits[ndig - 1] == '0')
  {
    digits[--ndig] = 0;
    scale = sat_add_i64 (scale, 1);
  }
  return write_candidate_parts (buf, bufsz, sign, digits, scale);
}

static bool same_float128 (const dtl_float128_t *orig, const char *s)
{
  dtl_float128_t parsed;
  return dtl_float128_from_string (s, &parsed) && memcmp (orig, &parsed, sizeof (*orig)) == 0;
}

static bool try_short_candidate (char *best, size_t bestsz, const dtl_float128_t *orig,
    bool sign, const struct bigint *coeff, int64_t scale)
{
  char cand[DTL_FLOAT128_STRING_BUFSZ];
  if (!format_decimal_candidate (cand, sizeof (cand), sign, coeff, scale))
    return false;
  if (!same_float128 (orig, cand))
    return true;
  if (*best == 0 || strlen (cand) < strlen (best) ||
      (strlen (cand) == strlen (best) && strchr (best, 'e') != NULL && strchr (cand, 'e') == NULL))
  {
    if (!write_candidate_string (best, bestsz, cand))
      return false;
  }
  return true;
}

struct binary_format {
  unsigned frac_bits;
  unsigned precision;
  unsigned decimal_digits;
  unsigned sign_bit;
  uint64_t exp_inf_nan;
  int exp_bias;
  int max_e;
  int min_normal_e;
  int min_subnormal_e;
};

static const struct binary_format binary32_format = {
  .frac_bits = 23,
  .precision = 24,
  .decimal_digits = FLT_DECIMAL_DIG,
  .sign_bit = 31,
  .exp_inf_nan = 0xffu,
  .exp_bias = 127,
  .max_e = 127,
  .min_normal_e = -126,
  .min_subnormal_e = -149
};

static const struct binary_format binary64_format = {
  .frac_bits = 52,
  .precision = 53,
  .decimal_digits = DBL_DECIMAL_DIG,
  .sign_bit = 63,
  .exp_inf_nan = 0x7ffu,
  .exp_bias = 1023,
  .max_e = 1023,
  .min_normal_e = -1022,
  .min_subnormal_e = -1074
};

static uint64_t binary_pack (const struct binary_format *fmt, bool sign, uint64_t exp, uint64_t frac)
{
  const uint64_t sign_bit = sign ? UINT64_C (1) << fmt->sign_bit : 0;
  const uint64_t frac_mask = (UINT64_C (1) << fmt->frac_bits) - 1;
  return sign_bit | (exp << fmt->frac_bits) | (frac & frac_mask);
}

static uint64_t f128_top_fraction_bits (uint64_t frac_hi, uint64_t frac_lo, unsigned n)
{
  assert (n <= 64);
  assert (n != 0);
  if (n <= 48)
    return frac_hi >> (48 - n);
  return (frac_hi << (n - 48)) | (frac_lo >> (64 - (n - 48)));
}

static uint64_t binary_pack_nan (const struct binary_format *fmt, bool sign, uint64_t frac_hi, uint64_t frac_lo)
{
  uint64_t frac = f128_top_fraction_bits (frac_hi, frac_lo, fmt->frac_bits);
  frac |= UINT64_C (1) << (fmt->frac_bits - 1);
  return binary_pack (fmt, sign, fmt->exp_inf_nan, frac);
}

static bool encode_rational_binary (bool sign, const struct bigint *a, const struct bigint *b,
    int64_t shift2, const struct binary_format *fmt, uint64_t *bits)
{
  int64_t e2;
  struct bigint sig;
  bigint_init (&sig);
  bool ok = false;

  if (bigint_is_zero (a))
  {
    *bits = binary_pack (fmt, sign, 0, 0);
    ok = true;
    goto done;
  }
  if (!floor_log2_rational (a, b, shift2, &e2))
    goto done;
  if (e2 > fmt->max_e)
  {
    *bits = binary_pack (fmt, sign, fmt->exp_inf_nan, 0);
    ok = true;
    goto done;
  }
  if (e2 < fmt->min_subnormal_e - 1)
  {
    *bits = binary_pack (fmt, sign, 0, 0);
    ok = true;
    goto done;
  }

  if (e2 < fmt->min_normal_e)
  {
    if (!round_scaled (a, b, sat_sub_i64 (shift2, fmt->min_subnormal_e), &sig))
      goto done;
    if (bigint_is_zero (&sig))
      *bits = binary_pack (fmt, sign, 0, 0);
    else if (bigint_bitlen (&sig) > fmt->frac_bits)
      *bits = binary_pack (fmt, sign, 1, 0);
    else
      *bits = binary_pack (fmt, sign, 0, bigint_bits64 (&sig, 0));
  }
  else
  {
    if (!round_scaled (a, b, sat_add_i64 (shift2, (int64_t) fmt->frac_bits - e2), &sig))
      goto done;
    if (bigint_bitlen (&sig) > fmt->precision)
    {
      if (!bigint_shift_right1 (&sig))
        goto done;
      e2++;
    }
    if (e2 > fmt->max_e)
      *bits = binary_pack (fmt, sign, fmt->exp_inf_nan, 0);
    else
    {
      assert (bigint_get_bit (&sig, fmt->frac_bits));
      sig.v[fmt->frac_bits / 32] &= ~((uint32_t) 1u << (fmt->frac_bits % 32));
      bigint_normalize (&sig);
      *bits = binary_pack (fmt, sign, (uint64_t) (e2 + fmt->exp_bias), bigint_bits64 (&sig, 0));
    }
  }
  ok = true;
done:
  bigint_fini (&sig);
  return ok;
}

static bool f128_to_binary (const dtl_float128_t *in, const struct binary_format *fmt, uint64_t *bits)
{
  const struct f128_bits b = f128_load_native (in);
  const bool sign = (b.hi >> 63) != 0;
  const uint16_t exp = (uint16_t) ((b.hi >> 48) & 0x7fff);
  const uint64_t frac_hi = b.hi & UINT64_C (0x0000ffffffffffff);
  const uint64_t frac_lo = b.lo;
  if (exp == F128_EXP_INF_NAN)
  {
    *bits = (frac_hi == 0 && frac_lo == 0) ?
        binary_pack (fmt, sign, fmt->exp_inf_nan, 0) :
        binary_pack_nan (fmt, sign, frac_hi, frac_lo);
    return true;
  }
  if (exp == 0 && frac_hi == 0 && frac_lo == 0)
  {
    *bits = binary_pack (fmt, sign, 0, 0);
    return true;
  }

  struct bigint m, one;
  bigint_init (&m);
  bigint_init (&one);
  bool ok = false;
  if (!bigint_from_significand (&m, frac_hi, frac_lo, exp != 0) || !bigint_set_u32 (&one, 1))
    goto done;
  const int64_t e = exp == 0 ? -16494 : (int64_t) exp - F128_EXP_BIAS - F128_FRAC_BITS;
  ok = encode_rational_binary (sign, &m, &one, e, fmt, bits);
done:
  bigint_fini (&m);
  bigint_fini (&one);
  return ok;
}

static bool encode_decimal_binary (bool sign, const struct bigint *coeff, int64_t scale,
    const struct binary_format *fmt, uint64_t *bits)
{
  struct bigint a, b;
  bigint_init (&a);
  bigint_init (&b);
  bool ok = false;
  if (scale >= 0)
  {
    if (!bigint_copy (&a, coeff) || !bigint_mul_pow5 (&a, (uint64_t) scale) || !bigint_set_u32 (&b, 1))
      goto done;
  }
  else
  {
    if (!bigint_copy (&a, coeff) || !bigint_set_u32 (&b, 1) || !bigint_mul_pow5 (&b, (uint64_t) -scale))
      goto done;
  }
  ok = encode_rational_binary (sign, &a, &b, scale, fmt, bits);
done:
  bigint_fini (&a);
  bigint_fini (&b);
  return ok;
}

static bool try_short_binary_candidate (char *best, size_t bestsz, uint64_t target_bits,
    const struct binary_format *fmt, bool sign, const struct bigint *coeff, int64_t scale)
{
  char cand[DTL_FLOAT128_STRING_BUFSZ];
  uint64_t bits;
  if (!format_decimal_candidate (cand, sizeof (cand), sign, coeff, scale))
    return false;
  if (!encode_decimal_binary (sign, coeff, scale, fmt, &bits))
    return false;
  if (bits != target_bits)
    return true;
  if (*best == 0 || strlen (cand) < strlen (best) ||
      (strlen (cand) == strlen (best) && strchr (best, 'e') != NULL && strchr (cand, 'e') == NULL))
  {
    if (!write_candidate_string (best, bestsz, cand))
      return false;
  }
  return true;
}

static dds_return_t print_shortest_binary (char *buf, size_t bufsz, uint64_t target_bits,
    const struct binary_format *fmt, bool sign, const struct bigint *m, int64_t e, int64_t d)
{
  char best[DTL_FLOAT128_STRING_BUFSZ] = "";
  struct bigint coeff;
  struct bigint near;
  bigint_init (&coeff);
  bigint_init (&near);
  dds_return_t rc = DDS_RETCODE_OUT_OF_RESOURCES;

  for (uint32_t ndigits = 1; ndigits <= fmt->decimal_digits; ndigits++)
  {
    const int64_t scale = sat_sub_i64 (d, (int64_t) ndigits - 1);
    if (!rounded_decimal_digits_n (m, e, d, ndigits, &coeff))
      goto done;
    if (!try_short_binary_candidate (best, sizeof (best), target_bits, fmt, sign, &coeff, scale))
      goto done;
    if (!bigint_is_zero (&coeff))
    {
      if (!bigint_copy (&near, &coeff))
        goto done;
      if (bigint_sub_small (&near, 1) &&
          !bigint_is_zero (&near) &&
          !try_short_binary_candidate (best, sizeof (best), target_bits, fmt, sign, &near, scale))
        goto done;
    }
    if (!bigint_copy (&near, &coeff) || !bigint_add_small (&near, 1))
      goto done;
    if (!try_short_binary_candidate (best, sizeof (best), target_bits, fmt, sign, &near, scale))
      goto done;
    if (best[0] != 0)
    {
      if (!write_candidate_string (buf, bufsz, best))
        rc = DDS_RETCODE_NOT_ENOUGH_SPACE;
      else
        rc = DDS_RETCODE_OK;
      goto done;
    }
  }
  rc = DDS_RETCODE_ERROR;
done:
  bigint_fini (&coeff);
  bigint_fini (&near);
  return rc;
}

static dds_return_t print_shortest_finite (char *buf, size_t bufsz, const dtl_float128_t *orig,
    bool sign, const struct bigint *m, int64_t e, int64_t d)
{
  char best[DTL_FLOAT128_STRING_BUFSZ] = "";
  struct bigint coeff;
  struct bigint near;
  bigint_init (&coeff);
  bigint_init (&near);
  dds_return_t rc = DDS_RETCODE_OUT_OF_RESOURCES;

  for (uint32_t ndigits = 1; ndigits <= F128_DECIMAL_DIGITS; ndigits++)
  {
    const int64_t scale = sat_sub_i64 (d, (int64_t) ndigits - 1);
    if (!rounded_decimal_digits_n (m, e, d, ndigits, &coeff))
      goto done;
    if (!try_short_candidate (best, sizeof (best), orig, sign, &coeff, scale))
      goto done;
    if (!bigint_is_zero (&coeff))
    {
      if (!bigint_copy (&near, &coeff))
        goto done;
      if (bigint_sub_small (&near, 1) &&
          !bigint_is_zero (&near) &&
          !try_short_candidate (best, sizeof (best), orig, sign, &near, scale))
        goto done;
    }
    if (!bigint_copy (&near, &coeff) || !bigint_add_small (&near, 1))
      goto done;
    if (!try_short_candidate (best, sizeof (best), orig, sign, &near, scale))
      goto done;
    if (best[0] != 0)
    {
      if (!write_candidate_string (buf, bufsz, best))
        rc = DDS_RETCODE_NOT_ENOUGH_SPACE;
      else
        rc = DDS_RETCODE_OK;
      goto done;
    }
  }
  rc = DDS_RETCODE_ERROR;
done:
  bigint_fini (&coeff);
  bigint_fini (&near);
  return rc;
}

dds_return_t dtl_float128_to_string (char *buf, size_t bufsz, const dtl_float128_t *in)
{
  const struct f128_bits b = f128_load_native (in);
  const bool sign = (b.hi >> 63) != 0;
  const uint16_t exp = (uint16_t) ((b.hi >> 48) & 0x7fff);
  const uint64_t frac_hi = b.hi & UINT64_C (0x0000ffffffffffff);
  const uint64_t frac_lo = b.lo;
  if (bufsz == 0)
    return DDS_RETCODE_NOT_ENOUGH_SPACE;
  if (exp == F128_EXP_INF_NAN)
  {
    const char *s = (frac_hi == 0 && frac_lo == 0) ? (sign ? "-inf" : "inf") : "nan";
    const size_t n = strlen (s);
    if (n + 1 > bufsz)
      return DDS_RETCODE_NOT_ENOUGH_SPACE;
    memcpy (buf, s, n + 1);
    return DDS_RETCODE_OK;
  }
  if (exp == 0 && frac_hi == 0 && frac_lo == 0)
  {
    const char *s = sign ? "-0" : "0";
    const size_t n = strlen (s);
    if (n + 1 > bufsz)
      return DDS_RETCODE_NOT_ENOUGH_SPACE;
    memcpy (buf, s, n + 1);
    return DDS_RETCODE_OK;
  }

  struct bigint m;
  bigint_init (&m);
  dds_return_t rc = DDS_RETCODE_OUT_OF_RESOURCES;
  if (!bigint_from_significand (&m, frac_hi, frac_lo, exp != 0))
    goto done;
  const int64_t e = exp == 0 ? -16494 : (int64_t) exp - F128_EXP_BIAS - F128_FRAC_BITS;
  int64_t d;
  if (!decimal_exponent (&m, e, &d))
    goto done;
  rc = print_shortest_finite (buf, bufsz, in, sign, &m, e, d);
done:
  bigint_fini (&m);
  return rc;
}

static dds_return_t dtl_float128_to_binary_string (char *buf, size_t bufsz, const dtl_float128_t *in,
    uint64_t target_bits, const struct binary_format *fmt)
{
  const struct f128_bits b = f128_load_native (in);
  const bool sign = (b.hi >> 63) != 0;
  const uint16_t exp = (uint16_t) ((b.hi >> 48) & 0x7fff);
  const uint64_t frac_hi = b.hi & UINT64_C (0x0000ffffffffffff);
  const uint64_t frac_lo = b.lo;
  if ((exp == F128_EXP_INF_NAN) || (exp == 0 && frac_hi == 0 && frac_lo == 0))
    return dtl_float128_to_string (buf, bufsz, in);

  struct bigint m;
  bigint_init (&m);
  dds_return_t rc = DDS_RETCODE_OUT_OF_RESOURCES;
  if (!bigint_from_significand (&m, frac_hi, frac_lo, exp != 0))
    goto done;
  const int64_t e = exp == 0 ? -16494 : (int64_t) exp - F128_EXP_BIAS - F128_FRAC_BITS;
  int64_t d;
  if (!decimal_exponent (&m, e, &d))
    goto done;
  rc = print_shortest_binary (buf, bufsz, target_bits, fmt, sign, &m, e, d);
done:
  bigint_fini (&m);
  return rc;
}

bool dtl_float32_from_string (const char *str, float *out)
{
  dtl_float128_t f128;
  uint64_t bits;
  uint32_t bits32;
  if (!dtl_float128_from_string (str, &f128) || !f128_to_binary (&f128, &binary32_format, &bits))
    return false;
  bits32 = (uint32_t) bits;
  memcpy (out, &bits32, sizeof (*out));
  return true;
}

bool dtl_float64_from_string (const char *str, double *out)
{
  dtl_float128_t f128;
  uint64_t bits;
  if (!dtl_float128_from_string (str, &f128) || !f128_to_binary (&f128, &binary64_format, &bits))
    return false;
  memcpy (out, &bits, sizeof (*out));
  return true;
}

dds_return_t dtl_float32_to_string (char *buf, size_t bufsz, float v)
{
  uint32_t bits;
  dtl_float128_t f128;
  memcpy (&bits, &v, sizeof (bits));
  dtl_float128_from_float32 (&f128, v);
  return dtl_float128_to_binary_string (buf, bufsz, &f128, bits, &binary32_format);
}

dds_return_t dtl_float64_to_string (char *buf, size_t bufsz, double v)
{
  uint64_t bits;
  dtl_float128_t f128;
  memcpy (&bits, &v, sizeof (bits));
  dtl_float128_from_float64 (&f128, v);
  return dtl_float128_to_binary_string (buf, bufsz, &f128, bits, &binary64_format);
}
