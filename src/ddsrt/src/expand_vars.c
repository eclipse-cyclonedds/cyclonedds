// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "dds/ddsrt/expand_vars.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/process.h"

// Limit expanded string size, this is really only used for configuration
// and so 10MB should be plenty for now
#define MAX_SIZE (10 * 1048576)

typedef char * (*expand_fn)(const char *src0, expand_lookup_fn lookup, void * data, uint32_t depth);

static void errorN (size_t n0, const char *s, const char *msg)
{
    const int n = (n0 > 100) ? 100 : (int) n0;
    DDS_ERROR("%*.*s%s: %s\n", n, n, s, ((size_t) n < n0) ? "..." : "", msg);
}

static void error (const char *s, const char *msg)
{
    errorN (strlen (s), s, msg);
}

static bool expand_append (char **dst, size_t *sz, size_t *pos, char c)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

static bool expand_append (char **dst, size_t *sz, size_t *pos, char c)
{
    if (*pos == *sz) {
        if (*sz >= MAX_SIZE) {
            return false;
        }
        *sz = (*sz < 1024) ? 1024 : (*sz * 2);
        *dst = ddsrt_realloc (*dst, *sz);
    }
    (*dst)[*pos] = c;
    (*pos)++;
    return true;
}

static char *expand_var (const char *name, char op, const char *alt, expand_fn expand, expand_lookup_fn lookup, void * data, uint32_t depth)
{
    const char *val = lookup (name, data);
    switch (op)
    {
        case 0:
            return ddsrt_strdup (val ? val : "");
        case '-':
            return val && *val ? ddsrt_strdup (val) : expand (alt, lookup, data, depth + 1);
        case '?':
            if (val && *val) {
                return ddsrt_strdup (val);
            } else {
                char *altx = expand (alt, lookup, data, depth + 1);
                if (altx) {
                    DDS_ERROR("%s: %s", name, altx);
                    ddsrt_free (altx);
                }
                return NULL;
            }
        case '+':
            return val && *val ? expand (alt, lookup, data, depth + 1) : ddsrt_strdup ("");
        default:
            abort ();
            return NULL;
    }
}

static char *expand_varbrace (const char **src, expand_fn expand, expand_lookup_fn lookup, void * data, uint32_t depth)
{
    const char *start = *src + 1;
    char *name, *x;
    assert (**src == '{');
    (*src)++;
    while (**src && **src != ':' && **src != '}') {
        (*src)++;
    }
    if (**src == 0) {
        goto err;
    }
    name = ddsrt_malloc ((size_t) (*src - start) + 1);
    memcpy (name, start, (size_t) (*src - start));
    name[*src - start] = 0;
    if (**src == '}') {
        (*src)++;
        x = expand_var (name, 0, NULL, expand, lookup, data, depth);
        ddsrt_free (name);
        return x;
    } else {
        const char *altstart;
        char *alt;
        char op;
        int nest = 0;
        assert (**src == ':');
        (*src)++;
        switch (**src) {
            case '-': case '+': case '?':
                op = **src;
                (*src)++;
                break;
            default:
                ddsrt_free(name);
                goto err;
        }
        altstart = *src;
        while (**src && (**src != '}' || nest > 0)) {
            if (**src == '{') {
                nest++;
            } else if (**src == '}') {
                assert (nest > 0);
                nest--;
            } else if (**src == '\\') {
                (*src)++;
                if (**src == 0) {
                    ddsrt_free(name);
                    goto err;
                }
            }
            (*src)++;
        }
        if (**src == 0) {
            ddsrt_free(name);
            goto err;
        }
        assert (**src == '}');
        alt = ddsrt_malloc ((size_t) (*src - altstart) + 1);
        memcpy (alt, altstart, (size_t) (*src - altstart));
        alt[*src - altstart] = 0;
        (*src)++;
        x = expand_var (name, op, alt, expand, lookup, data, depth);
        ddsrt_free (alt);
        ddsrt_free (name);
        return x;
    }
err:
    errorN((size_t) (*src - start), start, "invalid expansion");
    return NULL;
}

static char *expand_varsimple (const char **src, expand_fn expand, expand_lookup_fn lookup, void * data, uint32_t depth)
{
    const char *start = *src;
    char *name, *x;
    while (**src && (isalnum ((unsigned char) **src) || **src == '_')) {
        (*src)++;
    }
    assert (*src > start);
    name = ddsrt_malloc ((size_t) (*src - start) + 1);
    memcpy (name, start, (size_t) (*src - start));
    name[*src - start] = 0;
    x = expand_var (name, 0, NULL, expand, lookup, data, depth);
    ddsrt_free (name);
    return x;
}

static char *expand_varchar (const char **src, expand_fn expand, expand_lookup_fn lookup, void * data, uint32_t depth)
{
    char name[2];
    assert (**src);
    name[0] = **src;
    name[1] = 0;
    (*src)++;
    return expand_var (name, 0, NULL, expand, lookup, data, depth);
}

static char *ddsrt_expand_vars_sh1 (const char *src0, expand_lookup_fn lookup, void * data, uint32_t depth)
{
    /* Expands $X, ${X}, ${X:-Y}, ${X:+Y}, ${X:?Y} forms; $ and \ can be escaped with \ */
    if (depth >= 20)
    {
      error(src0, "variable expansions too deeply nested");
      return NULL;
    }
    const char *src = src0;
    size_t sz = strlen (src) + 1, pos = 0;
    char *dst = ddsrt_malloc (sz);
    while (*src) {
        if (*src == '\\') {
            src++;
            if (*src == 0) {
                error(src0, "incomplete escape at end of string");
                goto err;
            }
            if (!expand_append (&dst, &sz, &pos, *src++)) {
                error(src0, "result too large");
                goto err;
            }
        } else if (*src == '$') {
            char *x, *xp;
            src++;
            if (*src == 0) {
                error(src0, "incomplete variable expansion at end of string");
                goto err;
            } else if (*src == '{') {
                x = expand_varbrace (&src, &ddsrt_expand_vars_sh1, lookup, data, depth);
            } else if (isalnum ((unsigned char) *src) || *src == '_') {
                x = expand_varsimple (&src, &ddsrt_expand_vars_sh1, lookup, data, depth);
            } else {
                x = expand_varchar (&src, &ddsrt_expand_vars_sh1, lookup, data, depth);
            }
            if (x == NULL) {
                goto err;
            }
            xp = x;
            while (*xp) {
                if (!expand_append (&dst, &sz, &pos, *xp++)) {
                    error(src0, "result too large");
                    ddsrt_free(x);
                    goto err;
                }
            }
            ddsrt_free (x);
        } else {
            if (!expand_append (&dst, &sz, &pos, *src++)) {
                error(src0, "result too large");
                goto err;
            }
        }
    }
    if (!expand_append (&dst, &sz, &pos, 0)) {
        error(src0, "result too large");
        goto err;
    }
    return dst;
err:
    ddsrt_free(dst);
    return NULL;
}

static char *ddsrt_expand_vars1 (const char *src0, expand_lookup_fn lookup, void * data, uint32_t depth)
{
    /* Expands ${X}, ${X:-Y}, ${X:+Y}, ${X:?Y} forms, but not $X */
    if (depth >= 20)
    {
      error(src0, "variable expansions too deeply nested");
      return NULL;
    }
    const char *src = src0;
    size_t sz = strlen (src) + 1, pos = 0;
    char *dst = ddsrt_malloc (sz);
    while (*src) {
        if (*src == '$' && *(src + 1) == '{') {
            char *x, *xp;
            src++;
            x = expand_varbrace (&src, &ddsrt_expand_vars1, lookup, data, depth);
            if (x == NULL) {
                ddsrt_free(dst);
                return NULL;
            }
            xp = x;
            while (*xp) {
                if (!expand_append (&dst, &sz, &pos, *xp++)) {
                    error(src0, "result too large");
                    ddsrt_free (x);
                    goto err;
                }
            }
            ddsrt_free (x);
        } else {
            if (!expand_append (&dst, &sz, &pos, *src++)) {
                error(src0, "result too large");
                goto err;
            }
        }
    }
    if (!expand_append (&dst, &sz, &pos, 0)) {
        error(src0, "result too large");
        goto err;
    }
    return dst;
err:
    ddsrt_free(dst);
    return NULL;
}

char *ddsrt_expand_vars_sh (const char *src0, expand_lookup_fn lookup, void * data)
{
  return ddsrt_expand_vars_sh1 (src0, lookup, data, 0);
}

char *ddsrt_expand_vars (const char *src0, expand_lookup_fn lookup, void * data)
{
  return ddsrt_expand_vars1 (src0, lookup, data, 0);
}

