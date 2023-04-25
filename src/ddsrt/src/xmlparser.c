// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/xmlparser.h"

#define TOK_EOF -1
#define TOK_OPEN_TAG -2
#define TOK_ID -3
#define TOK_STRING -4
#define TOK_CLOSE_TAG -5
#define TOK_SHORTHAND_CLOSE_TAG -6
#define TOK_ERROR -7
#define TOK_CDATA -8

#define NOMARKER (~(size_t)0)

static const char *cdata_magic = "<![CDATA[";

struct ddsrt_xmlp_state {
    size_t cbufp; /* current position in cbuf */
    size_t cbufn; /* number of bytes in cbuf (cbufp <= cbufn) */
    size_t cbufmax; /* allocated size of cbuf (cbufn <= cbufmax) */
    size_t cbufmark; /* NORMARKER or marker position (cbufmark <= cbufp) for rewinding */
    int eof; /* fake EOF (for treating missing close tags as EOF) */
    unsigned char *cbuf; /* parser input buffer */
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
    unsigned options;
    struct ddsrt_xmlp_callbacks cb; /* user-supplied callbacks (or stubs) */
};

static int cb_null_elem_open (void *varg, uintptr_t parentinfo, uintptr_t *eleminfo, const char *name, int line)
{
    DDSRT_UNUSED_ARG (varg);
    DDSRT_UNUSED_ARG (parentinfo);
    DDSRT_UNUSED_ARG (eleminfo);
    DDSRT_UNUSED_ARG (name);
    DDSRT_UNUSED_ARG (line);
    return 0;
}

static int cb_null_attr (void *varg, uintptr_t eleminfo, const char *name, const char *value, int line)
{
    DDSRT_UNUSED_ARG (varg);
    DDSRT_UNUSED_ARG (eleminfo);
    DDSRT_UNUSED_ARG (name);
    DDSRT_UNUSED_ARG (value);
    DDSRT_UNUSED_ARG (line);
    return 0;
}

static int cb_null_elem_data (void *varg, uintptr_t eleminfo, const char *data, int line)
{
    DDSRT_UNUSED_ARG (varg);
    DDSRT_UNUSED_ARG (eleminfo);
    DDSRT_UNUSED_ARG (data);
    DDSRT_UNUSED_ARG (line);
    return 0;
}

static int cb_null_elem_close (void *varg, uintptr_t eleminfo, int line)
{
    DDSRT_UNUSED_ARG (varg);
    DDSRT_UNUSED_ARG (eleminfo);
    DDSRT_UNUSED_ARG (line);
    return 0;
}

static void cb_null_error (void *varg, const char *msg, int line)
{
    DDSRT_UNUSED_ARG (varg);
    DDSRT_UNUSED_ARG (msg);
    DDSRT_UNUSED_ARG (line);
}

static void ddsrt_xmlp_new_common (struct ddsrt_xmlp_state *st)
{
    st->cbufp = 0;
    st->cbufmark = NOMARKER;
    st->eof = 0;
    st->tpp = 0;
    st->tpescp = 0;
    st->tpsz = 1024;
    st->tp = ddsrt_malloc (st->tpsz);
    st->line = 1;
    st->prevline = 1;
    st->linemark = 0;
    st->peektok = 0;
    st->peekpayload = NULL;
    st->nest = 0;
    st->error = 0;
    st->options = DDSRT_XMLP_REQUIRE_EOF;
}

static void ddsrt_xmlp_new_setCB (struct ddsrt_xmlp_state *st, void *varg, const struct ddsrt_xmlp_callbacks *cb)
{
    st->varg = varg;
    st->cb = *cb;
    if (st->cb.attr == 0) st->cb.attr = cb_null_attr;
    if (st->cb.elem_open == 0) st->cb.elem_open = cb_null_elem_open;
    if (st->cb.elem_data == 0) st->cb.elem_data = cb_null_elem_data;
    if (st->cb.elem_close == 0) st->cb.elem_close = cb_null_elem_close;
    if (st->cb.error == 0) st->cb.error = cb_null_error;
}

struct ddsrt_xmlp_state *ddsrt_xmlp_new_file (FILE *fp, void *varg, const struct ddsrt_xmlp_callbacks *cb)
{
    struct ddsrt_xmlp_state *st;
    st = ddsrt_malloc (sizeof (*st));
    st->cbufn = 0;
    st->cbufmax = 8192;
    st->cbuf = ddsrt_malloc (st->cbufmax);
    st->fp = fp;
    ddsrt_xmlp_new_common (st);
    ddsrt_xmlp_new_setCB (st, varg, cb);
    return st;
}

struct ddsrt_xmlp_state *ddsrt_xmlp_new_string (const char *string, void *varg, const struct ddsrt_xmlp_callbacks *cb)
{
    struct ddsrt_xmlp_state *st;
    st = ddsrt_malloc (sizeof (*st));
    st->cbufn = strlen (string);
    st->cbufmax = st->cbufn;
    st->cbuf = (unsigned char *) string;
    st->fp = NULL;
    ddsrt_xmlp_new_common (st);
    ddsrt_xmlp_new_setCB (st, varg, cb);
    return st;
}

void ddsrt_xmlp_set_options (struct ddsrt_xmlp_state *st, unsigned options)
{
    st->options = options;
}

size_t ddsrt_xmlp_get_bufpos (const struct ddsrt_xmlp_state *st)
{
    return st->cbufp;
}

void ddsrt_xmlp_free (struct ddsrt_xmlp_state *st)
{
    if (st->fp != NULL) {
        ddsrt_free (st->cbuf);
    }
    ddsrt_free (st->tp);
    ddsrt_free (st);
}

static int make_chars_available (struct ddsrt_xmlp_state *st, size_t nmin)
{
    size_t n, pos;
    if (st->eof) {
        return 0;
    }
    pos = (st->cbufmark != NOMARKER) ? st->cbufmark : st->cbufp;
    assert (st->cbufn >= st->cbufp);
    assert (st->cbufmax >= st->cbufn);
    assert (st->cbufmark == NOMARKER || st->cbufmark <= st->cbufp);
    /* fast-path available chars */
    if (st->cbufn - st->cbufp >= nmin) {
        return 1;
    }
    /* ensure buffer space is available */
    if (st->fp != NULL) {
        if (pos + nmin > st->cbufmax) {
            memmove (st->cbuf, st->cbuf + pos, st->cbufn - pos);
            st->cbufn -= pos;
            st->cbufp -= pos;
            if (st->cbufmark != NOMARKER) {
                st->cbufmark -= pos;
            }
        }
        /* buffer is owned by caller if fp = NULL, and by us if fp != NULL */
        if (st->cbufmax < st->cbufp + nmin) {
            st->cbufmax = st->cbufp + nmin;
            st->cbuf = ddsrt_realloc (st->cbuf, st->cbufmax);
        }
        /* try to refill buffer if a backing file is present; eof (or end-of-string) is
         reached when this doesn't add any bytes to the buffer */
        n = fread (st->cbuf + st->cbufn, 1, st->cbufmax - st->cbufn, st->fp);
        st->cbufn += n;
    }
    return (st->cbufn - st->cbufp >= nmin);
}

static void set_input_marker (struct ddsrt_xmlp_state *st)
{
    assert (st->cbufmark == NOMARKER);
    st->cbufmark = st->cbufp;
    st->linemark = st->line;
}

static void discard_input_marker (struct ddsrt_xmlp_state *st)
{
    assert (st->cbufmark != NOMARKER);
    st->cbufmark = NOMARKER;
    st->linemark = 0;
}

static int have_input_marker (struct ddsrt_xmlp_state *st)
{
    return (st->cbufmark != NOMARKER);
}

static void rewind_to_input_marker (struct ddsrt_xmlp_state *st)
{
    assert (st->cbufmark != NOMARKER);
    st->cbufp = st->cbufmark;
    st->line = st->linemark;
    discard_input_marker (st);
}

static int next_char (struct ddsrt_xmlp_state *st)
{
    unsigned char c;
    if (!make_chars_available (st, 1)) {
        return TOK_EOF;
    }
    c = st->cbuf[st->cbufp++];
    if (c == '\n') {
        st->line++;
    }
    return c;
}

static int peek_char (struct ddsrt_xmlp_state *st)
{
    if (!make_chars_available (st, 1)) {
        return TOK_EOF;
    }
    return st->cbuf[st->cbufp];
}

static int peek_chars (struct ddsrt_xmlp_state *st, const char *seq, int consume)
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

static int unescape_insitu (char *buffer, size_t *n)
{
    DDSRT_WARNING_MSVC_OFF(4996);
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
    DDSRT_WARNING_MSVC_ON(4996);
}

static void discard_payload (struct ddsrt_xmlp_state *st)
{
    st->tpp = 0;
    st->tpescp = 0;
}

static int append_to_payload (struct ddsrt_xmlp_state *st, int c, int islit)
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
        st->tp = ddsrt_realloc (st->tp, st->tpsz);
    }
    return 0;
}

static int save_payload (char **payload, struct ddsrt_xmlp_state *st, int trim)
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
        p = ddsrt_strdup("");
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
            p = ddsrt_strdup("");
        } else {
            p = ddsrt_malloc (last - first + 2);
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

static int next_token_ident (struct ddsrt_xmlp_state *st, char **payload)
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

static int next_token_tag_withoutclose (struct ddsrt_xmlp_state *st, char **payload)
{
    if (peek_chars (st, "<![CDATA[", 0)) {
        return next_char (st);
    } else {
        int tok = TOK_OPEN_TAG;
        /* pre: peek_char(st) == '<' */
        (void) next_char (st);
        if (peek_char (st) == '/') {
            tok = TOK_CLOSE_TAG;
            (void) next_char (st);
        }
        /* we only do tag names that are identifiers */
        if (peek_char (st) == '>' && (st->options & DDSRT_XMLP_ANONYMOUS_CLOSE_TAG)) {
            return TOK_SHORTHAND_CLOSE_TAG;
        } else if (!qq_isidentfirst (peek_char (st))) {
            return TOK_ERROR;
        } else {
            next_token_ident (st, payload);
            return tok;
        }
    }
}

static int next_token_string (struct ddsrt_xmlp_state *st, char **payload, const char *endm)
{
    /* positioned at first character of string */
    while (!peek_chars (st, endm, 0) && peek_char (st) != TOK_EOF) {
        if (append_to_payload (st, next_char (st), 0) < 0) {
            return TOK_ERROR;
        }
    }
    if (!peek_chars (st, endm, 1)) {
        discard_payload (st);
        return TOK_ERROR;
    } else if (save_payload (payload, st, 0) < 0) {
        return TOK_ERROR;
    } else {
        return TOK_STRING;
    }
}

static int skip_comment (struct ddsrt_xmlp_state *st)
{
    if (!peek_chars (st, "<!--", 1)) {
        return 0;
    }
    while (peek_char (st) != TOK_EOF && (peek_char (st) != '-' || !peek_chars (st, "-->", 0))) {
        (void) next_char (st);
    }
    if (peek_chars (st, "-->", 1)) {
        return 1;
    } else {
        return TOK_ERROR;
    }
}

static void processing_instruction (struct ddsrt_xmlp_state *st, const char *end)
{
    /* just after <?; skip everything up to and include ?> */
    while (peek_char (st) != TOK_EOF && !peek_chars (st, end, 1)) {
        (void) next_char (st);
    }
}

static void drop_peek_token (struct ddsrt_xmlp_state *st)
{
    st->peektok = 0;
    if (st->peekpayload) {
        ddsrt_free (st->peekpayload);
        st->peekpayload = NULL;
    }
}

static int next_token (struct ddsrt_xmlp_state *st, char **payload)
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
            ddsrt_free (st->peekpayload);
            st->peekpayload = NULL;
        }
        return tok;
    } else {
        int cmt, tok;
        st->prevline = st->line;
        do {
            while (qq_isspace (peek_char (st))) {
                (void) next_char (st);
            }
        } while ((cmt = skip_comment (st)) > 0);
        if (cmt == TOK_ERROR) {
            tok = TOK_ERROR;
        } else if (peek_chars (st, "<?", 1)) {
            processing_instruction (st, "?>");
            return next_token (st, payload);
        } else if (peek_chars (st, cdata_magic, 0)) {
            tok = TOK_CDATA;
        } else if (peek_chars (st, "<!", 1)) {
            processing_instruction (st, ">");
            return next_token (st, payload);
        } else {
            int n = peek_char (st);
            if (n == '<') {
                tok = next_token_tag_withoutclose (st, payload);
            } else if (n == '"' || n == '\'') {
                const unsigned char c = (unsigned char) next_char (st);
                const unsigned char endm[2] = { c, 0 };
                tok = next_token_string (st, payload, (const char *) endm);
            } else if (n == 0xe2 && (peek_chars (st, "\xe2\x80\x9c", 0) || peek_chars (st, "\xe2\x80\x98", 0))) {
                /* allow fancy unicode quotes (U+201c .. U+201d and U+2018 ... U+2019) in
                   UTF-8 representation because email clients like to rewrite plain ones,
                   and this has caused people trouble several times already */
                (void) next_char (st); (void) next_char (st);
                const unsigned char c = (unsigned char) next_char (st);
                const unsigned char endm[4] = { 0xe2, 0x80, (unsigned char) (c + 1), 0 };
                tok = next_token_string (st, payload, (const char *) endm);
            } else if (qq_isidentfirst (n)) {
                tok = next_token_ident (st, payload);
            } else if (peek_chars (st, "/>", 1)) {
                tok = TOK_SHORTHAND_CLOSE_TAG;
            } else {
                tok = next_char (st);
            }
        }
        if (tok == TOK_ERROR) {
            char msg[512];
            (void) snprintf (msg, sizeof (msg), "invalid token encountered");
            st->cb.error (st->varg, msg, st->line);
            st->error = 1;
        }
        return tok;
    }
}

static int peek_token (struct ddsrt_xmlp_state *st)
{
    int tok;
    char *payload;
    tok = next_token (st, &payload);
    st->peektok = tok;
    st->peekpayload = payload;
    return tok;
}

static int parse_element (struct ddsrt_xmlp_state *st, uintptr_t parentinfo)
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

    if ((ret = st->cb.elem_open (st->varg, parentinfo, &eleminfo, name, st->line)) < 0) {
        PE_ERROR ("failed in element open callback", name);
    }

    while (peek_token (st) == TOK_ID) {
        char *content;
        next_token (st, &aname);
        if (next_token (st, NULL) != '=') {
            PE_LOCAL_ERROR ("expecting '=' following attribute name", aname);
        }
        if (next_token (st, &content) != TOK_STRING) {
            ddsrt_free (content);
            PE_LOCAL_ERROR ("expecting string value for attribute", aname);
        }
        ret = st->cb.attr (st->varg, eleminfo, aname, content, st->line);
        ddsrt_free (content);
        if (ret < 0) {
            PE_ERROR2 ("failed in attribute callback", name, aname);
        }
        ddsrt_free (aname);
        aname = NULL;
    }

    tok = next_token (st, NULL);
    switch (tok)
    {
        case TOK_SHORTHAND_CLOSE_TAG:
            ret = st->cb.elem_close (st->varg, eleminfo, st->line);
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
                    if (*content != '\0') {
                        ret = st->cb.elem_data (st->varg, eleminfo, content, st->line);
                        ddsrt_free (content);
                        if (ret < 0) {
                            PE_ERROR ("failed in data callback", 0);
                        }
                    } else {
                        ddsrt_free (content);
                    }
                }
            }
            st->nest--;
            set_input_marker (st);
            if (((tok = next_token (st, &ename)) != TOK_CLOSE_TAG && tok != TOK_SHORTHAND_CLOSE_TAG) || next_char (st) != '>') {
                if (!(st->options & DDSRT_XMLP_MISSING_CLOSE_AS_EOF)) {
                  PE_LOCAL_ERROR ("expecting closing tag", name);
                } else {
                    rewind_to_input_marker (st);
                    st->eof = 1;
                    tok = TOK_SHORTHAND_CLOSE_TAG;
                }
            }
            if (tok != TOK_SHORTHAND_CLOSE_TAG && strcmp (name, ename) != 0) {
                PE_LOCAL_ERROR ("open/close tag mismatch", ename);
            }
            if (have_input_marker (st)) {
                discard_input_marker (st);
            }
            ret = st->cb.elem_close (st->varg, eleminfo, st->line);
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
    ddsrt_free (name);
    ddsrt_free (aname);
    ddsrt_free (ename);
    return ret;
#undef PE_LOCAL_ERROR
#undef PE_ERROR
#undef PE_ERROR2
}

int ddsrt_xmlp_parse (struct ddsrt_xmlp_state *st)
{
    if (peek_token (st) == TOK_EOF) {
        return 0;
    } else {
        int ret = parse_element (st, 0);
        if (ret < 0|| !(st->options & DDSRT_XMLP_REQUIRE_EOF) || next_token (st, NULL) == TOK_EOF ) {
            return ret;
        } else {
            return -1;
        }
    }
}
