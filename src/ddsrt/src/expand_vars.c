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

typedef char * (*expand_fn)(const char *src0, expand_lookup_fn lookup, void * data);

static void expand_append (char **dst, size_t *sz, size_t *pos, char c)
{
    if (*pos == *sz) {
        *sz += 1024;
        *dst = ddsrt_realloc (*dst, *sz);
    }
    (*dst)[*pos] = c;
    (*pos)++;
}

static char *expand_var (const char *name, char op, const char *alt, expand_fn expand, expand_lookup_fn lookup, void * data)
{
    const char *val = lookup (name, data);
    switch (op)
    {
        case 0:
            return ddsrt_strdup (val ? val : "");
        case '-':
            return val && *val ? ddsrt_strdup (val) : expand (alt, lookup, data);
        case '?':
            if (val && *val) {
                return ddsrt_strdup (val);
            } else {
                char *altx = expand (alt, lookup, data);
                DDS_LOG (DDS_LC_ERROR, "%s: %s\n", name, altx);
                ddsrt_free (altx);
                return NULL;
            }
        case '+':
            return val && *val ? expand (alt, lookup, data) : ddsrt_strdup ("");
        default:
            abort ();
            return NULL;
    }
}

static char *expand_varbrace (const char **src, expand_fn expand, expand_lookup_fn lookup, void * data)
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
        x = expand_var (name, 0, NULL, expand, lookup, data);
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
        x = expand_var (name, op, alt, expand, lookup, data);
        ddsrt_free (alt);
        ddsrt_free (name);
        return x;
    }
err:
    DDS_ERROR("%*.*s: invalid expansion\n", (int) (*src - start), (int) (*src - start), start);
    return NULL;
}

static char *expand_varsimple (const char **src, expand_fn expand, expand_lookup_fn lookup, void * data)
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
    x = expand_var (name, 0, NULL, expand, lookup, data);
    ddsrt_free (name);
    return x;
}

static char *expand_varchar (const char **src, expand_fn expand, expand_lookup_fn lookup, void * data)
{
    char name[2];
    assert (**src);
    name[0] = **src;
    name[1] = 0;
    (*src)++;
    return expand_var (name, 0, NULL, expand, lookup, data);
}

char *ddsrt_expand_vars_sh (const char *src0, expand_lookup_fn lookup, void * data)
{
    /* Expands $X, ${X}, ${X:-Y}, ${X:+Y}, ${X:?Y} forms; $ and \ can be escaped with \ */
    const char *src = src0;
    size_t sz = strlen (src) + 1, pos = 0;
    char *dst = ddsrt_malloc (sz);
    while (*src) {
        if (*src == '\\') {
            src++;
            if (*src == 0) {
                DDS_ERROR("%s: incomplete escape at end of string\n", src0);
                ddsrt_free(dst);
                return NULL;
            }
            expand_append (&dst, &sz, &pos, *src++);
        } else if (*src == '$') {
            char *x, *xp;
            src++;
            if (*src == 0) {
                DDS_ERROR("%s: incomplete variable expansion at end of string\n", src0);
                ddsrt_free(dst);
                return NULL;
            } else if (*src == '{') {
                x = expand_varbrace (&src, &ddsrt_expand_vars_sh, lookup, data);
            } else if (isalnum ((unsigned char) *src) || *src == '_') {
                x = expand_varsimple (&src, &ddsrt_expand_vars_sh, lookup, data);
            } else {
                x = expand_varchar (&src, &ddsrt_expand_vars_sh, lookup, data);
            }
            if (x == NULL) {
                ddsrt_free(dst);
                return NULL;
            }
            xp = x;
            while (*xp) {
                expand_append (&dst, &sz, &pos, *xp++);
            }
            ddsrt_free (x);
        } else {
            expand_append (&dst, &sz, &pos, *src++);
        }
    }
    expand_append (&dst, &sz, &pos, 0);
    return dst;
}

char *ddsrt_expand_vars (const char *src0, expand_lookup_fn lookup, void * data)
{
    /* Expands ${X}, ${X:-Y}, ${X:+Y}, ${X:?Y} forms, but not $X */
    const char *src = src0;
    size_t sz = strlen (src) + 1, pos = 0;
    char *dst = ddsrt_malloc (sz);
    while (*src) {
        if (*src == '$' && *(src + 1) == '{') {
            char *x, *xp;
            src++;
            x = expand_varbrace (&src, &ddsrt_expand_vars, lookup, data);
            if (x == NULL) {
                ddsrt_free(dst);
                return NULL;
            }
            xp = x;
            while (*xp) {
                expand_append (&dst, &sz, &pos, *xp++);
            }
            ddsrt_free (x);
        } else {
            expand_append (&dst, &sz, &pos, *src++);
        }
    }
    expand_append (&dst, &sz, &pos, 0);
    return dst;
}

