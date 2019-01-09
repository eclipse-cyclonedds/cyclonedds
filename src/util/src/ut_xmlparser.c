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
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "os/os.h"
#include "util/ut_xmlparser.h"

#define TOK_EOF -1
#define TOK_OPEN_TAG -2
#define TOK_ID -3
#define TOK_STRING -4
#define TOK_CLOSE_TAG -5
#define TOK_SHORTHAND_CLOSE_TAG -6
#define TOK_ERROR -7

#define NOMARKER (~(size_t)0)

struct ut_xmlpState {
    size_t cbufp; /* current position in cbuf */
    size_t cbufn; /* number of bytes in cbuf (cbufp <= cbufn) */
    size_t cbufmax; /* allocated size of cbuf (cbufn <= cbufmax) */
    size_t cbufmark; /* NORMARKER or marker position (cbufmark <= cbufp) for rewinding */
    char *cbuf; /* parser input buffer */
    FILE *fp; /* file to refill cbuf from, or NULL if parsing a string */
    int line; /* current line number */
    int prevline; /* line number at last token */
    int linemark; /* line number at marker */
    int peektok; /* token lookahead (peek token when no next token available) */
    char *peekpayload; /* payload associated with lookahead */
    int error; /* error flag to call error callback only once */
    size_t tpp; /* length of token payload */
    size_t tpsz; /* allocated size of tp */
    char *tp; /* token payload buffer */
    size_t tpescp; /* still escape sequences in tpescp .. tpp */
    int nest; /* current nesting level */
    void *varg; /* user argument to callback functions */
    struct ut_xmlpCallbacks cb; /* user-supplied callbacks (or stubs) */
};

static int cb_null_elem_open (void *varg, uintptr_t parentinfo, uintptr_t *eleminfo, const char *name)
{
    OS_UNUSED_ARG (varg);
    OS_UNUSED_ARG (parentinfo);
    OS_UNUSED_ARG (eleminfo);
    OS_UNUSED_ARG (name);
    return 0;
}

static int cb_null_attr (void *varg, uintptr_t eleminfo, const char *name, const char *value)
{
    OS_UNUSED_ARG (varg);
    OS_UNUSED_ARG (eleminfo);
    OS_UNUSED_ARG (name);
    OS_UNUSED_ARG (value);
    return 0;
}

static int cb_null_elem_data (void *varg, uintptr_t eleminfo, const char *data)
{
    OS_UNUSED_ARG (varg);
    OS_UNUSED_ARG (eleminfo);
    OS_UNUSED_ARG (data);
    return 0;
}

static int cb_null_elem_close (void *varg, uintptr_t eleminfo)
{
    OS_UNUSED_ARG (varg);
    OS_UNUSED_ARG (eleminfo);
    return 0;
}

static void cb_null_error (void *varg, const char *msg, int line)
{
    OS_UNUSED_ARG (varg);
    OS_UNUSED_ARG (msg);
    OS_UNUSED_ARG (line);
}

static void ut_xmlpNewCommon (struct ut_xmlpState *st)
{
    st->cbufp = 0;
    st->cbufmark = NOMARKER;
    st->tpp = 0;
    st->tpescp = 0;
    st->tpsz = 1024;
    st->tp = os_malloc (st->tpsz);
    st->line = 1;
    st->prevline = 1;
    st->linemark = 0;
    st->peektok = 0;
    st->peekpayload = NULL;
    st->nest = 0;
    st->error = 0;
}

static void ut_xmlpNewSetCB (struct ut_xmlpState *st, void *varg, const struct ut_xmlpCallbacks *cb)
{
    st->varg = varg;
    st->cb = *cb;
    if (st->cb.attr == 0) st->cb.attr = cb_null_attr;
    if (st->cb.elem_open == 0) st->cb.elem_open = cb_null_elem_open;
    if (st->cb.elem_data == 0) st->cb.elem_data = cb_null_elem_data;
    if (st->cb.elem_close == 0) st->cb.elem_close = cb_null_elem_close;
    if (st->cb.error == 0) st->cb.error = cb_null_error;
}

struct ut_xmlpState *ut_xmlpNewFile (FILE *fp, void *varg, const struct ut_xmlpCallbacks *cb)
{
    struct ut_xmlpState *st;
    st = os_malloc (sizeof (*st));
    st->cbufn = 0;
    st->cbufmax = 8192;
    st->cbuf = os_malloc (st->cbufmax);
    st->fp = fp;
    ut_xmlpNewCommon (st);
    ut_xmlpNewSetCB (st, varg, cb);
    return st;
}

struct ut_xmlpState *ut_xmlpNewString (const char *string, void *varg, const struct ut_xmlpCallbacks *cb)
{
    struct ut_xmlpState *st;
    st = os_malloc (sizeof (*st));
    st->cbufn = strlen (string);
    st->cbufmax = st->cbufn;
    st->cbuf = (char *) string;
    st->fp = NULL;
    ut_xmlpNewCommon (st);
    ut_xmlpNewSetCB (st, varg, cb);
    return st;
}

void ut_xmlpFree (struct ut_xmlpState *st)
{
    if (st->fp != NULL) {
        os_free (st->cbuf);
    }
    os_free (st->tp);
    os_free (st);
}

static int make_chars_available (struct ut_xmlpState *st, size_t nmin)
{
    size_t n, pos;
    pos = (st->cbufmark != NOMARKER) ? st->cbufmark : st->cbufp;
    assert (st->cbufn >= st->cbufp);
    assert (st->cbufmax >= st->cbufn);
    assert (st->cbufmark == NOMARKER || st->cbufmark <= st->cbufp);
    /* fast-path available chars */
    if (st->cbufn - st->cbufp >= nmin) {
        return 1;
    }
    /* ensure buffer space is available */
    if (pos + nmin > st->cbufmax) {
        memmove (st->cbuf, st->cbuf + pos, st->cbufn - pos);
        st->cbufn -= pos;
        st->cbufp -= pos;
        if (st->cbufmark != NOMARKER) {
            st->cbufmark -= pos;
        }
    }
    /* buffer is owned by caller if fp = NULL, and by us if fp != NULL */
    if (st->cbufp + st->cbufmax < nmin && st->fp != NULL) {
        st->cbufmax = st->cbufp + nmin;
        st->cbuf = os_realloc (st->cbuf, st->cbufmax);
    }
    /* try to refill buffer if a backing file is present; eof (or end-of-string) is
       reached when this doesn't add any bytes to the buffer */
    if (st->fp != NULL) {
        n = fread (st->cbuf + st->cbufn, 1, st->cbufmax - st->cbufn, st->fp);
        st->cbufn += n;
    }
    return (st->cbufn - st->cbufp >= nmin);
}

static void set_input_marker (struct ut_xmlpState *st)
{
    assert (st->cbufmark == NOMARKER);
    st->cbufmark = st->cbufp;
    st->linemark = st->line;
}

static void discard_input_marker (struct ut_xmlpState *st)
{
    assert (st->cbufmark != NOMARKER);
    st->cbufmark = NOMARKER;
    st->linemark = 0;
}

static void rewind_to_input_marker (struct ut_xmlpState *st)
{
    assert (st->cbufmark != NOMARKER);
    st->cbufp = st->cbufmark;
    st->line = st->linemark;
    discard_input_marker (st);
}

static int next_char (struct ut_xmlpState *st)
{
    char c;
    if (!make_chars_available (st, 1)) {
        return TOK_EOF;
    }
    c = st->cbuf[st->cbufp++];
    if (c == '\n') {
        st->line++;
    }
    return c;
}

static int peek_char (struct ut_xmlpState *st)
{
    if (!make_chars_available (st, 1)) {
        return TOK_EOF;
    }
    return st->cbuf[st->cbufp];
}

static int peek_chars (struct ut_xmlpState *st, const char *seq, int consume)
{
    size_t n = strlen (seq);
    if (!make_chars_available (st, n)) {
        return 0;
    }
    if (memcmp (st->cbuf + st->cbufp, seq, n) != 0) {
        return 0;
    } else {
        if (consume) st->cbufp += n;
        return 1;
    }
}

static int qq_isspace (int x)
{
    return x == ' ' || x == '\t' || x == '\v' || x == '\r' || x == '\n';
}

static int qq_isidentfirst (int x)
{
    return (x >= 'A' && x <= 'Z') || (x >= 'a' && x <= 'z');
}

static int qq_isidentcont (int x)
{
    return qq_isidentfirst (x) || (x >= '0' && x <= '9') || x == '_' || x == '-' || x == ':';
}

static char *unescape_into_utf8 (char *dst, unsigned cp)
{
    if (cp < 0x80) {
        *dst++ = (char) cp;
    } else if (cp <= 0x7ff) {
        *dst++ = (char) ((cp >> 6) + 0xc0);
        *dst++ = (char) ((cp & 0x3f) + 0x80);
    } else if (cp <= 0xffff) {
        *dst++ = (char) ((cp >> 12) + 0xe0);
        *dst++ = (char) (((cp >> 6) & 0x3f) + 0x80);
        *dst++ = (char) ((cp & 0x3f) + 0x80);
    } else if (cp <= 0x10ffff) {
        *dst++ = (char) ((cp >> 18) + 0xf0);
        *dst++ = (char) (((cp >> 12) & 0x3f) + 0x80);
        *dst++ = (char) (((cp >> 6) & 0x3f) + 0x80);
        *dst++ = (char) ((cp & 0x3f) + 0x80);
    } else {
        dst = NULL;
    }
    return dst;
}

OS_WARNING_MSVC_OFF(4996);
static int unescape_insitu (char *buffer, size_t *n)
{
    const char *src = buffer;
    char const * const srcend = buffer + *n;
    char *dst = buffer;
    while (src < srcend)
    {
        if (*src != '&') {
            *dst++ = *src++;
        } else if (src + 1 == srcend) {
            return -1;
        } else {
            char tmp[16], *ptmp = tmp;
            src++;
            while (ptmp < tmp + sizeof (tmp) && src < srcend) {
                char c = *src++;
                *ptmp++ = c;
                if (c == ';') {
                    break;
                }
            }
            if (ptmp == tmp || *(ptmp-1) != ';') {
                return -1;
            }
            *--ptmp = 0;
            if (tmp[0] == '#') {
                unsigned cp;
                int pos;
                if (sscanf (tmp, "#x%x%n", &cp, &pos) == 1 && tmp[pos] == 0) {
                    ;
                } else if (sscanf (tmp, "#%u%n", &cp, &pos) == 1 && tmp[pos] == 0) {
                    ;
                } else {
                    return -1;
                }
                if ((dst = unescape_into_utf8(dst, cp)) == NULL) {
                    return -1;
                }
            } else if (strcmp (tmp, "lt") == 0) {
                *dst++ = '<';
            } else if (strcmp (tmp, "gt") == 0) {
                *dst++ = '>';
            } else if (strcmp (tmp, "amp") == 0) {
                *dst++ = '&';
            } else if (strcmp (tmp, "apos") == 0) {
                *dst++ = '\'';
            } else if (strcmp (tmp, "quot") == 0) {
                *dst++ = '"';
            } else {
                return -1;
            }
        }
    }
    *n = (size_t) (dst - buffer);
    return 0;
}
OS_WARNING_MSVC_ON(4996);

static void discard_payload (struct ut_xmlpState *st)
{
    st->tpp = 0;
    st->tpescp = 0;
}

static int append_to_payload (struct ut_xmlpState *st, int c, int islit)
{
    if (!islit) {
        st->tp[st->tpp++] = (char) c;
    } else {
        if (st->tpescp < st->tpp) {
            size_t n = st->tpp - st->tpescp;
            if (unescape_insitu (st->tp + st->tpescp, &n) < 0) {
                discard_payload (st);
                return -1;
            }
            st->tpp = st->tpescp + n;
        }
        st->tp[st->tpp++] = (char) c;
        st->tpescp = st->tpp;
    }
    if (st->tpp == st->tpsz) {
        st->tpsz += 1024;
        st->tp = os_realloc (st->tp, st->tpsz);
    }
    return 0;
}

static int save_payload (char **payload, struct ut_xmlpState *st, int trim)
{
    char *p;
    if (st->tpescp < st->tpp) {
        size_t n = st->tpp - st->tpescp;
        if (unescape_insitu (st->tp + st->tpescp, &n) < 0) {
            discard_payload (st);
            return -1;
        }
        st->tpp = st->tpescp + n;
    }
    if (payload == NULL) {
        p = NULL;
    } else if (st->tpp == 0) {
        p = os_strdup("");
    } else {
        size_t first = 0, last = st->tpp - 1;
        if (trim) {
            while (first <= last && qq_isspace (st->tp[first])) {
                first++;
            }
            while (first <= last && qq_isspace (st->tp[last]) && last > 0) {
                last--;
            }
        }
        if (first > last) {
            p = os_strdup("");
        } else {
            p = os_malloc (last - first + 2);
            /* Could be improved, parser error will be "invalid char sequence" if malloc fails. */
            memcpy (p, st->tp + first, last - first + 1);
            p[last - first + 1] = 0;
        }
    }
    discard_payload (st);
    if (payload) {
        *payload = p;
    }
    return 0;
}

static int next_token_ident (struct ut_xmlpState *st, char **payload)
{
    while (qq_isidentcont (peek_char (st))) {
        if (append_to_payload (st, next_char (st), 0) < 0) {
            return TOK_ERROR;
        }
    }
    if (save_payload (payload, st, 0) < 0) {
        return TOK_ERROR;
    } else {
        return TOK_ID;
    }
}

static int next_token_tag_withoutclose (struct ut_xmlpState *st, char **payload)
{
    if (peek_chars (st, "<![CDATA[", 0)) {
        return next_char (st);
    } else {
        int tok = TOK_OPEN_TAG;
        /* pre: peek_char(st) == '<' */
        next_char (st);
        if (peek_char (st) == '/') {
            tok = TOK_CLOSE_TAG;
            next_char (st);
        }
        /* we only do tag names that are identifiers */
        if (!qq_isidentfirst (peek_char (st))) {
            return TOK_ERROR;
        }
        next_token_ident (st, payload);
        return tok;
    }
}

static int next_token_string (struct ut_xmlpState *st, char **payload)
{
    /* pre: peek_char(st) == ('"' or '\'') */
    int endm = next_char (st);
    while (peek_char (st) != endm && peek_char (st) != TOK_EOF) {
        if (append_to_payload (st, next_char (st), 0) < 0) {
            return TOK_ERROR;
        }
    }
    if (next_char (st) != endm) {
        discard_payload (st);
        return TOK_ERROR;
    } else if (save_payload (payload, st, 0) < 0) {
        return TOK_ERROR;
    } else {
        return TOK_STRING;
    }
}

static int skip_comment (struct ut_xmlpState *st)
{
    if (!peek_chars (st, "<!--", 1)) {
        return 0;
    }
    while ((peek_char (st) != TOK_EOF && peek_char (st) != '-') || !peek_chars (st, "-->", 0)) {
        next_char (st);
    }
    if (peek_chars (st, "-->", 1)) {
        return 1;
    } else {
        return TOK_ERROR;
    }
}

static void processing_instruction (struct ut_xmlpState *st, const char *end)
{
    /* just after <?; skip everything up to and include ?> */
    while (peek_char (st) != TOK_EOF && !peek_chars (st, end, 1)) {
        next_char (st);
    }
}

static void drop_peek_token (struct ut_xmlpState *st)
{
    st->peektok = 0;
    if (st->peekpayload) {
        os_free (st->peekpayload);
        st->peekpayload = NULL;
    }
}

static int next_token (struct ut_xmlpState *st, char **payload)
{
    /* Always return a valid pointer to allocated memory or a null
     pointer, regardless of token type */
    if (payload) {
        *payload = NULL;
    }
    if (st->error) {
        return TOK_ERROR;
    } else if (st->peektok) {
        int tok = st->peektok;
        st->peektok = 0;
        if (payload) {
            *payload = st->peekpayload;
        } else if (st->peekpayload) {
            os_free (st->peekpayload);
            st->peekpayload = NULL;
        }
        return tok;
    } else {
        int cmt, tok;
        st->prevline = st->line;
        do {
            while (qq_isspace (peek_char (st))) {
                next_char (st);
            }
        } while ((cmt = skip_comment (st)) > 0);
        if (cmt == TOK_ERROR) {
            tok = TOK_ERROR;
        } else if (peek_chars (st, "<?", 1)) {
            processing_instruction (st, "?>");
            return next_token (st, payload);
        } else if (peek_chars (st, "<!", 1)) {
            processing_instruction (st, ">");
            return next_token (st, payload);
        } else {
            int n = peek_char (st);
            if (n == '<') {
                tok = next_token_tag_withoutclose (st, payload);
            } else if (n == '"' || n == '\'') {
                tok = next_token_string (st, payload);
            } else if (qq_isidentfirst (n)) {
                tok = next_token_ident (st, payload);
            } else if (peek_chars (st, "/>", 1)) {
                tok = TOK_SHORTHAND_CLOSE_TAG;
            } else {
                tok = next_char (st);
            }
        }
        if (tok == TOK_ERROR) {
            st->error = 1;
        }
        return tok;
    }
}

static int peek_token (struct ut_xmlpState *st)
{
    int tok;
    char *payload;
    tok = next_token (st, &payload);
    st->peektok = tok;
    st->peekpayload = payload;
    return tok;
}

static int parse_element (struct ut_xmlpState *st, uintptr_t parentinfo)
{
#define PE_ERROR2(c,c1,c2) do { errc = (c); errc1 = (c1); errc2 = (c2); goto err; } while (0)
#define PE_ERROR(c,c1) PE_ERROR2(c,c1,0)
#define PE_LOCAL_ERROR(c,c1) do { ret = -1; PE_ERROR ((c), (c1)); } while (0)
    char *name = NULL, *aname = NULL, *ename = NULL;
    const char *errc = NULL, *errc1 = NULL, *errc2 = NULL;
    uintptr_t eleminfo;
    int ret = 0, tok;

    if (next_token (st, &name) != TOK_OPEN_TAG) {
        PE_LOCAL_ERROR ("expecting '<'", 0);
    }

    if ((ret = st->cb.elem_open (st->varg, parentinfo, &eleminfo, name)) < 0) {
        PE_ERROR ("failed in element open callback", name);
    }

    while (peek_token (st) == TOK_ID) {
        char *content;
        next_token (st, &aname);
        if (next_token (st, NULL) != '=') {
            PE_LOCAL_ERROR ("expecting '=' following attribute name", aname);
        }
        if (next_token (st, &content) != TOK_STRING) {
            os_free (content);
            PE_LOCAL_ERROR ("expecting string value for attribute", aname);
        }
        ret = st->cb.attr (st->varg, eleminfo, aname, content);
        os_free (content);
        if (ret < 0) {
            PE_ERROR2 ("failed in attribute callback", name, aname);
        }
        os_free (aname);
        aname = NULL;
    }

    tok = next_token (st, NULL);
    switch (tok)
    {
        case TOK_SHORTHAND_CLOSE_TAG:
            ret = st->cb.elem_close (st->varg, eleminfo);
            goto ok;
        case '>':
            st->nest++;
            set_input_marker (st);
            if (peek_token (st) == TOK_OPEN_TAG) {
                /* child elements */
                discard_input_marker (st);
                while (peek_token (st) == TOK_OPEN_TAG) {
                    if ((ret = parse_element (st, eleminfo)) < 0) {
                        PE_ERROR ("parse children", 0);
                    }
                }
            } else {
                /* text */
                static const char *cdata_magic = "<![CDATA[";
                char *content;
                int cmt = 0;
                rewind_to_input_marker (st);
                drop_peek_token (st);
                do {
                    /* gobble up content until EOF or markup */
                    while (peek_char (st) != '<' && peek_char (st) != TOK_EOF) {
                        if (append_to_payload (st, next_char (st), 0) < 0) {
                            PE_LOCAL_ERROR ("invalid character sequence", 0);
                        }
                    }
                    /* if the mark-up happens to be a CDATA, consume it, and gobble up characters
                       until the closing marker is reached, which then also gets consumed */
                    if (peek_chars (st, cdata_magic, 1)) {
                        while (!peek_chars (st, "]]>", 1) && peek_char (st) != TOK_EOF) {
                            if (append_to_payload (st, next_char (st), 1) < 0) {
                                PE_LOCAL_ERROR ("invalid character sequence", 0);
                            }
                        }
                    }
                    /* then, if the markup is a comment, skip it and try again */
                } while ((peek_char (st) != '<' || (cmt = skip_comment (st)) > 0) && make_chars_available (st, sizeof(cdata_magic) - 1));
                if (cmt == TOK_ERROR) {
                    discard_payload (st);
                    PE_LOCAL_ERROR ("invalid comment", 0);
                }
                if (save_payload (&content, st, 1) < 0) {
                    PE_ERROR ("invalid character sequence", 0);
                } else if (content != NULL) {
                    if(*content != '\0') {
                        ret = st->cb.elem_data (st->varg, eleminfo, content);
                        os_free (content);
                        if (ret < 0) {
                            PE_ERROR ("failed in data callback", 0);
                        }
                    } else {
                        os_free (content);
                    }
                }
            }
            st->nest--;
            if (next_token (st, &ename) != TOK_CLOSE_TAG || next_char (st) != '>') {
                PE_LOCAL_ERROR ("expecting closing tag", name);
            }
            if (strcmp (name, ename) != 0) {
                PE_LOCAL_ERROR ("open/close tag mismatch", ename);
            }
            ret = st->cb.elem_close (st->varg, eleminfo);
            goto ok;
        default:
            PE_LOCAL_ERROR ("expecting '/>' or '>'", 0);
    }

err:
    if (!st->error) {
        char msg[512];
        (void) snprintf (msg, sizeof (msg), "%s (%s%s%s)", errc, errc1 ? errc1 : "", errc1 && errc2 ? ", " : "", errc2 ? errc2 : "");
        st->cb.error (st->varg, msg, st->prevline);
        st->error = 1;
    }
ok:
    os_free (name);
    os_free (aname);
    os_free (ename);
    return ret;
#undef PE_LOCAL_ERROR
#undef PE_ERROR
#undef PE_ERROR2
}

int ut_xmlpParse (struct ut_xmlpState *st)
{
    if (peek_token (st) == TOK_EOF) {
        return 0;
    } else {
        int ret = parse_element (st, 0);
        if (ret < 0 || next_token (st, NULL) == TOK_EOF) {
            return ret;
        } else {
            return -1;
        }
    }
}

int ut_xmlUnescapeInsitu (char *buffer, size_t *n) {
    return unescape_insitu (buffer, n);
}
