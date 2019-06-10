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

#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/process.h"

typedef char * (*expand_fn)(const char *src0);

static void expand_append (char **dst, size_t *sz, size_t *pos, char c)
{
    if (*pos == *sz) {
        *sz += 1024;
        *dst = ddsrt_realloc (*dst, *sz);
    }
    (*dst)[*pos] = c;
    (*pos)++;
}

static char *expand_env (const char *name, char op, const char *alt, expand_fn expand)
{
    char pidstr[20];
    char *env = NULL;

    if (name[0] == '$' && name[1] == 0) {
        snprintf (pidstr, sizeof (pidstr), "%"PRIdPID, ddsrt_getpid ());
        env = pidstr;
    } else {
        (void) ddsrt_getenv (name, &env);
    }

    switch (op)
    {
        case 0:
            return ddsrt_strdup (env ? env : "");
        case '-':
            return env && *env ? ddsrt_strdup (env) : expand (alt);
        case '?':
            if (env && *env) {
                return ddsrt_strdup (env);
            } else {
                char *altx = expand (alt);
                DDS_ERROR("%s: %s\n", name, altx);
                ddsrt_free (altx);
                return NULL;
            }
        case '+':
            return env && *env ? expand (alt) : ddsrt_strdup ("");
        default:
            abort ();
            return NULL;
    }
}

static char *expand_envbrace (const char **src, expand_fn expand)
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
        x = expand_env (name, 0, NULL, expand);
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
        x = expand_env (name, op, alt, expand);
        ddsrt_free (alt);
        ddsrt_free (name);
        return x;
    }
err:
    DDS_ERROR("%*.*s: invalid expansion\n", (int) (*src - start), (int) (*src - start), start);
    return NULL;
}

static char *expand_envsimple (const char **src, expand_fn expand)
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
    x = expand_env (name, 0, NULL, expand);
    ddsrt_free (name);
    return x;
}

static char *expand_envchar (const char **src, expand_fn expand)
{
    char name[2];
    assert (**src);
    name[0] = **src;
    name[1] = 0;
    (*src)++;
    return expand_env (name, 0, NULL, expand);
}

char *ddsrt_expand_envvars_sh (const char *src0)
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
                x = expand_envbrace (&src, &ddsrt_expand_envvars_sh);
            } else if (isalnum ((unsigned char) *src) || *src == '_') {
                x = expand_envsimple (&src, &ddsrt_expand_envvars_sh);
            } else {
                x = expand_envchar (&src, &ddsrt_expand_envvars_sh);
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

char *ddsrt_expand_envvars (const char *src0)
{
    /* Expands ${X}, ${X:-Y}, ${X:+Y}, ${X:?Y} forms, but not $X */
    const char *src = src0;
    size_t sz = strlen (src) + 1, pos = 0;
    char *dst = ddsrt_malloc (sz);
    while (*src) {
        if (*src == '$' && *(src + 1) == '{') {
            char *x, *xp;
            src++;
            x = expand_envbrace (&src, &ddsrt_expand_envvars);
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
