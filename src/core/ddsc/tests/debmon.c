// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <ctype.h>
#include <string.h>

#include "dds/dds.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/strtod.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__debmon.h"
#include "ddsi__ipaddr.h"
#include "test_common.h"

struct bytebuf {
  char *data;
  size_t size;
  size_t cap;
};

static void bytebuf_append (struct bytebuf *buf, const char *data, size_t size)
{
  if (buf->data == NULL || size + 1 > buf->cap - buf->size)
  {
    size_t cap = buf->cap ? buf->cap : 1024;
    while (buf->size + size + 1 > cap)
      cap *= 2;
    buf->data = ddsrt_realloc (buf->data, cap);
    buf->cap = cap;
  }
  memcpy (buf->data + buf->size, data, size);
  buf->size += size;
  buf->data[buf->size] = 0;
}

static void bytebuf_fini (struct bytebuf *buf)
{
  ddsrt_free (buf->data);
}

static socklen_t sockaddr_len (const struct sockaddr_storage *addr)
{
  switch (addr->ss_family)
  {
    case AF_INET:
      return (socklen_t) sizeof (struct sockaddr_in);
#if DDSRT_HAVE_IPV6
    case AF_INET6:
      return (socklen_t) sizeof (struct sockaddr_in6);
#endif
    default:
      CU_FAIL_FATAL ("unsupported debmon locator address family");
      return 0;
  }
}

static void read_debmon_response (const ddsi_locator_t *loc, struct bytebuf *response)
{
  struct sockaddr_storage addr;
  ddsi_ipaddr_from_loc (&addr, loc);

  ddsrt_socket_t sock;
  dds_return_t rc = ddsrt_socket (&sock, addr.ss_family, SOCK_STREAM, 0);
  CU_ASSERT_EQ_FATAL (rc, DDS_RETCODE_OK);

  rc = ddsrt_connect (sock, (const struct sockaddr *) &addr, sockaddr_len (&addr));
  CU_ASSERT_EQ_FATAL (rc, DDS_RETCODE_OK);

  char chunk[1024];
  size_t n;
  do {
    rc = ddsrt_recv (sock, chunk, sizeof (chunk), 0, &n);
    CU_ASSERT (rc == DDS_RETCODE_OK || rc == DDS_RETCODE_NO_CONNECTION);
    if (rc == DDS_RETCODE_OK && n > 0)
      bytebuf_append (response, chunk, n);
  } while (rc == DDS_RETCODE_OK && n > 0);

  rc = ddsrt_close (sock);
  CU_ASSERT_EQ_FATAL (rc, DDS_RETCODE_OK);
}

static unsigned hexval (char c)
{
  if (c >= '0' && c <= '9')
    return (unsigned) (c - '0');
  if (c >= 'a' && c <= 'f')
    return (unsigned) (c - 'a' + 10);
  if (c >= 'A' && c <= 'F')
    return (unsigned) (c - 'A' + 10);
  CU_FAIL_FATAL ("invalid chunk length");
  return 0;
}

static void dechunk_response_body (const char *body, size_t size, struct bytebuf *dechunked)
{
  size_t off = 0;
  while (off < size)
  {
    while (off + 1 < size && body[off] == '\r' && body[off + 1] == '\n')
      off += 2;

    size_t chunk_size = 0, digits = 0;
    while (off < size && isxdigit ((unsigned char) body[off])) {
      chunk_size = 16 * chunk_size + hexval (body[off++]);
      digits++;
    }
    CU_ASSERT_GT_FATAL (digits, 0);
    CU_ASSERT_LT_FATAL (off + 1, size);
    CU_ASSERT_FATAL (body[off] == '\r' && body[off + 1] == '\n');
    off += 2;

    if (off + 1 < size && body[off] == '\r' && body[off + 1] == '\n')
      off += 2;
    if (chunk_size == 0)
      return;

    CU_ASSERT_LEQ_FATAL (chunk_size, size - off);
    bytebuf_append (dechunked, body + off, chunk_size);
    off += chunk_size;
  }

  CU_FAIL_FATAL ("missing terminating chunk");
}

static bool skip_json_string (const char **p)
{
  assert (**p == '"');
  (*p)++;
  while ((*p)[0] && (*p)[0] != '"') {
    if ((*p)[0] != '\\') {
      (*p)++;
    } else {
      if ((*p)[1] == 0)
        return false;
      (*p) += 2;
    }
  }
  if (**p == 0)
    return false;
  (*p)++;
  return true;
}

static bool skip_json_scalar (const char **p)
{
  if (strncmp (*p, "true", 4) == 0) { *p += 4; return true; }
  if (strncmp (*p, "false", 5) == 0) { *p += 5; return true; }
  if (strncmp (*p, "null", 4) == 0) { *p += 4; return true; }
  char *end;
  double v;
  if (ddsrt_strtod (*p, &end, &v) == DDS_RETCODE_OK && end >= *p) {
    *p = end; return true;
  }
  return false;
}

static bool not_json (void)
{
  return false;
}

static bool looks_like_json (const char *json)
{
  const char *p = json;
  while (*p && isspace ((unsigned char) *p))
    p++;
  if (*p != '{')
    return not_json ();
  ++p;
  enum ctx_kind { CTX_OBJECT, CTX_ARRAY };
  enum expect { EXP_VALUE, EXP_KEY_OR_END, EXP_COLON, EXP_COMMA_OR_END };
  struct ctx {
    enum ctx_kind kind;
    enum expect expect;
  } stack[64] = {
    { .kind = CTX_OBJECT, .expect = EXP_KEY_OR_END }, // dummy
    { .kind = CTX_OBJECT, .expect = EXP_KEY_OR_END }
  }, *sp = &stack[1];
  size_t count = 0;

  while (sp > &stack[0])
  {
    while (*p && isspace ((unsigned char) *p))
      p++;
    switch (sp->expect)
    {
      case EXP_COMMA_OR_END:
        switch (*p) {
          case ',':
            ++p;
            sp->expect = (sp->kind == CTX_OBJECT) ? EXP_KEY_OR_END : EXP_VALUE;
            break;
          case '}':
            if (sp->kind != CTX_OBJECT)
              return not_json ();
            ++p;
            --sp;
            break;
          case ']':
            if (sp->kind != CTX_ARRAY)
              return not_json ();
            ++p;
            --sp;
            break;
          default:
            return not_json ();
        }
        break;
      case EXP_KEY_OR_END:
        switch (*p) {
          case '"':
            if (!skip_json_string (&p))
              return not_json ();
            sp->expect = EXP_COLON;
            break;
          case '}':
            ++p;
            --sp;
            break;
          default:
            return not_json ();
        }
        break;
      case EXP_COLON:
        if (*p != ':')
          return not_json ();
        ++p;
        sp->expect = EXP_VALUE;
        break;
      case EXP_VALUE:
        sp->expect = EXP_COMMA_OR_END;
        if (*p == '{' || *p == '[') {
          CU_ASSERT_LT_FATAL (sp + 1, &stack[sizeof (stack) / sizeof (stack[0])]);
          ++sp;
          sp->kind = (*p == '{') ? CTX_OBJECT : CTX_ARRAY;
          sp->expect = (*p == '{') ? EXP_KEY_OR_END : EXP_VALUE;
          ++p;
        } else if (*p == ']' && sp->kind == CTX_ARRAY) {
          --sp;
          ++p;
        } else if (*p == '"') {
          if (!skip_json_string (&p))
            return not_json ();
          count++;
        } else {
          if (!skip_json_scalar (&p))
            return not_json ();
          count++;
        }
        break;
    }
  }
  while (*p && isspace ((unsigned char) *p))
    p++;
  if (*p == 0 && count > 0)
    return true;
  return not_json ();
}

static dds_entity_t create_domain_for_debmon (dds_domainid_t domainid)
{
  const char *config_base =
    "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}"
    "<Internal>"
    "  <MonitorPort>0</MonitorPort>"
    "</Internal>"
    "<Discovery>"
    "  <ExternalDomainId>0</ExternalDomainId>"
    "  <Tag>${CYCLONEDDS_PID}</Tag>"
    "</Discovery>";
  char *config = ddsrt_expand_envvars (config_base, domainid);
  const dds_entity_t dom = dds_create_domain (domainid, config);
  CU_ASSERT_GT_FATAL (dom, 0);
  ddsrt_free (config);
  return dom;
}

struct debmon_entities {
  dds_entity_t pp;
  dds_entity_t rd;
  dds_entity_t wr;
};

static struct debmon_entities create_entities (dds_domainid_t domainid, const char *topic_name)
{
  struct debmon_entities ents;
  ents.pp = dds_create_participant (domainid, NULL, NULL);
  CU_ASSERT_GT_FATAL (ents.pp, 0);
  const dds_entity_t tp = dds_create_topic (ents.pp, &Space_Type1_desc, topic_name, NULL, NULL);
  CU_ASSERT_GT_FATAL (tp, 0);
  ents.rd = dds_create_reader (ents.pp, tp, NULL, NULL);
  CU_ASSERT_GT_FATAL (ents.rd, 0);
  ents.wr = dds_create_writer (ents.pp, tp, NULL, NULL);
  CU_ASSERT_GT_FATAL (ents.wr, 0);
  return ents;
}

CU_Test (ddsc_debmon, smell)
{
  char topic_name[100];
  create_unique_topic_name ("ddsc_debmon_smell", topic_name, sizeof (topic_name));

  const dds_entity_t dom0 = create_domain_for_debmon (0);
  struct ddsi_domaingv *gv0 = get_domaingv (dom0);
  CU_ASSERT_NEQ_FATAL (gv0, NULL);

  const dds_entity_t dom1 = create_domain_for_debmon (1);

  struct debmon_entities ents0 = create_entities (0, topic_name);
  struct debmon_entities ents1 = create_entities (1, topic_name);

  sync_reader_writer (ents1.pp, ents1.rd, ents0.pp, ents0.wr);
  sync_reader_writer (ents0.pp, ents0.rd, ents1.pp, ents1.wr);

  struct ddsi_domaingv *gv1 = get_domaingv (dom1);
  CU_ASSERT_NEQ_FATAL (gv1, NULL);

  ddsi_locator_t locs[2];
  CU_ASSERT_FATAL (ddsi_get_debug_monitor_locator (gv0->debmon, &locs[0]));
  CU_ASSERT_FATAL (ddsi_get_debug_monitor_locator (gv1->debmon, &locs[1]));

  for (size_t i = 0; i < sizeof (locs) / sizeof (locs[0]); i++)
  {
    struct bytebuf response = { 0 };
    struct bytebuf body = { 0 };
    read_debmon_response (&locs[i], &response);

    CU_ASSERT_NEQ_FATAL (response.data, NULL);

    // The contents of the header does not have to remain this simple
    static const char header[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
    const size_t header_len = strlen (header);
    CU_ASSERT_EQ_FATAL (strncmp (response.data, header, header_len), 0);

    dechunk_response_body (response.data + header_len, response.size - header_len, &body);
    CU_ASSERT_FATAL (looks_like_json (body.data));

    bytebuf_fini (&body);
    bytebuf_fini (&response);
  }

  dds_return_t rc;
  rc = dds_delete (dom1);
  CU_ASSERT_EQ_FATAL (rc, DDS_RETCODE_OK);
  rc = dds_delete (dom0);
  CU_ASSERT_EQ_FATAL (rc, DDS_RETCODE_OK);
}
