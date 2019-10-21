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

#include "cyclonedds/ddsrt/environ.h"
#include "cyclonedds/ddsrt/heap.h"
#include "cyclonedds/ddsrt/log.h"
#include "cyclonedds/ddsrt/string.h"
#include "cyclonedds/ddsrt/process.h"

typedef char * (*expand_fn)(const char *src0, uint32_t domid);

static void expand_append (char **dst, size_t *sz, size_t *pos, char c)
{
    if (*pos == *sz) {
        *sz += 1024;
        *dst = ddsrt_realloc (*dst, *sz);
    }
    (*dst)[*pos] = c;
    (*pos)++;
}

static char *expand_env (const char *name, char op, const char *alt, expand_fn expand, uint32_t domid)
{
    char idstr[20];
    char *env = NULL;
    dds_return_t ret;

    if ((ret = ddsrt_getenv (name, &env)) == DDS_RETCODE_OK) {
        /* ok */
    } else if (strcmp (name, "$") == 0 || strcmp (name, "CYCLONEDDS_PID") == 0) {
        snprintf (idstr, sizeof (idstr), "%"PRIdPID, ddsrt_getpid ());
        env = idstr;
    } else if (strcmp (name, "CYCLONEDDS_DOMAIN_ID") == 0 && domid != UINT32_MAX) {
        snprintf (idstr, sizeof (idstr), "%"PRIu32, domid);
        env = idstr;
    }

    switch (op)
    {
        case 0:
            return ddsrt_strdup (env ? env : "");
        case '-':
            return env && *env ? ddsrt_strdup (env) : expand (alt, domid);
        case '?':
            if (env && *env) {
                return ddsrt_strdup (env);
            } else {
                char *altx = expand (alt, domid);
                DDS_ILOG (DDS_LC_ERROR, domid, "%s: %s\n", name, altx);
                ddsrt_free (altx);
                return NULL;
            }
        case '+':
            return env && *env ? expand (alt, domid) : ddsrt_strdup ("");
        default:
            abort ();
            return NULL;
    }
}

static char *expand_envbrace (const char **src, expand_fn expand, uint32_t domid)
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
        x = expand_env (name, 0, NULL, expand, domid);
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
        x = expand_env (name, op, alt, expand, domid);
        ddsrt_free (alt);
        ddsrt_free (name);
        return x;
    }
err:
    DDS_ERROR("%*.*s: invalid expansion\n", (int) (*src - start), (int) (*src - start), start);
    return NULL;
}

static char *expand_envsimple (const char **src, expand_fn expand, uint32_t domid)
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
    x = expand_env (name, 0, NULL, expand, domid);
    ddsrt_free (name);
    return x;
}

static char *expand_envchar (const char **src, expand_fn expand, uint32_t domid)
{
    char name[2];
    assert (**src);
    name[0] = **src;
    name[1] = 0;
    (*src)++;
    return expand_env (name, 0, NULL, expand, domid);
}

char *ddsrt_expand_envvars_sh (const char *src0, uint32_t domid)
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
                x = expand_envbrace (&src, &ddsrt_expand_envvars_sh, domid);
            } else if (isalnum ((unsigned char) *src) || *src == '_') {
                x = expand_envsimple (&src, &ddsrt_expand_envvars_sh, domid);
            } else {
                x = expand_envchar (&src, &ddsrt_expand_envvars_sh, domid);
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

char *ddsrt_expand_envvars (const char *src0, uint32_t domid)
{
    /* Expands ${X}, ${X:-Y}, ${X:+Y}, ${X:?Y} forms, but not $X */
    const char *src = src0;
    size_t sz = strlen (src) + 1, pos = 0;
    char *dst = ddsrt_malloc (sz);
    while (*src) {
        if (*src == '$' && *(src + 1) == '{') {
            char *x, *xp;
            src++;
            x = expand_envbrace (&src, &ddsrt_expand_envvars, domid);
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
