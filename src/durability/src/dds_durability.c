/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include "durablesupport.h"
#include "dds/durability/dds_durability_private.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/avl.h"
#include "ddsc/dds.h"
#include "../src/dds__writer.h"
#include "../src/dds__reader.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include <string.h>

#define DEFAULT_QOURUM                       1
#define DEFAULT_IDENT                        "durable_support"

/* maximum supported topic name size
 * Topic names larger than this size are not supported  */
#define MAX_TOPIC_NAME_SIZE                  255

#define DC_UNUSED_ARG(x)                     ((void)x)

#define DC_FLAG_SET_NOT_FOUND                (((uint32_t)0x01) << 16)
#define DC_FLAG_SET_BEGIN                    (((uint32_t)0x01) << 17)
#define DC_FLAG_SET_END                      (((uint32_t)0x01) << 18)

#define TRACE(...) DDS_CLOG (DDS_LC_DUR, &domaingv->logconfig, __VA_ARGS__)

/************* start of common functions ****************/
/* LH:
 * The following functions are duplicated from their equivalents used by the ds
 * In future we might want to create a common library that is used both by ds and dc.
 */

static char *dc_stringify_id(const DurableSupport_id_t id, char *buf)
{
  assert(buf);
  snprintf (buf, 37, "%02x%02x%02x%02x\055%02x%02x\055%02x%02x\055%02x%02x\055%02x%02x%02x%02x%02x%02x",
                id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7],
                id[8], id[9], id[10], id[11], id[12], id[13], id[14], id[15]);
  return buf;
}

static char *dc_responsetype_image (DurableSupport_responsetype_t type)
{
  switch (type) {
    case DurableSupport_RESPONSETYPE_SET:
      return "set";
    case DurableSupport_RESPONSETYPE_READER:
      return "reader";
    case DurableSupport_RESPONSETYPE_DATA:
      return "data";
    default:
      return "?";
  }
}

static int dc_blob_image (dds_sequence_octet blob, char *buf, size_t n)
{
  uint32_t nrofbytes = 2 * blob._length + 1;  /* the number of bytes to fill when the complete blob is stored, including the '\0' */
  uint32_t j;
  size_t i = 0;
  int l;

  if (buf == NULL) {
    /* no space allocated for display */
    return -1;
  }
  if (n == 0) {
    /* do not print anything */
    return 0;
  }
  if (nrofbytes <= n) {
    /* the complete blob fits in buf */
    for (j=0; j < blob._length; j++) {
      if ((l = snprintf(buf+i, n-i, "%02x", (uint8_t)blob._buffer[j])) < 0) {
        goto err;
      }
      assert(l < (int)(n-i));  /* never truncated */
      i += (size_t)l;
    }
  } else {
    /* The blob does not fit in buf.
     * if n < 5 then we display '..';
     * if n >=5 and n < 7 we display 'AB..'
     * if n > 7 we display 'AB..XY' (where the length of the head and tail varies depending on buf size n)
     */
    if (n < 5) {
      if ((l = snprintf(buf+i, n-i, "..")) < 0) {
        goto err;
      }
      i += (size_t)l;
    } else if (n < 7) {
      if ((l = snprintf(buf+i, n-i, "%02x..", (uint8_t)blob._buffer[0])) < 0) {
        goto err;
      }
    } else {
      /* calculate head and tail part */
      size_t np = ((n-1) - 2) / 4;
      size_t k;
      /* print head */
      for (k=0; k < np; k++) {
        if ((l = snprintf(buf+i, n-i, "%02x", (uint8_t)blob._buffer[k])) < 0) {
          goto err;
        }
        assert(l < (int)(n-i));    /* impossible to overflow buf */
        i += (size_t)l;
      }
      /* print separator '..' */
      if ((l = snprintf(buf+i, n-i, "%s", "..")) < 0) {
        goto err;
      }
      assert(l < (int)(n-i));    /* impossible to overflow buf */
      i += (size_t)l;
      /* print tail */
      for (k=0; k < np; k++) {
        if ((l = snprintf(buf+i, n-i, "%02x", (uint8_t)blob._buffer[blob._length - np + k])) < 0) {
          goto err;
        }
        assert(l < (int)(n-i));   /* impossible to overflow buf */
        i += (size_t)l;
      }
    }
  }
  l = (int)i;
  return (l >= (int)n) ? 0 /* no more space in buf */ : l;

err:
  return -1;
}

static int dc_stringify_request_key (char *buf, size_t n, const DurableSupport_request_key *key)
{
  size_t i = 0;
  int l;
  char rguid_str[37];

  if (buf == NULL) {
    goto err;
  }
  if (key == NULL) {
    buf[0] = '\0';
    return 0;
  }
  if ((l = snprintf(buf+i, n-i, "\"key\":{\"rguid\":\"%s\"}", dc_stringify_id(key->rguid, rguid_str))) < 0) {
    goto err;
  }
  i += (size_t)l;
  if (i >= n) {
    /* truncated */
    buf[n-1] = '\0';
  }
  return (int)i;
err:
  DDS_ERROR("dc_stringify_request_key failed");
  return -1;
}

static int dc_stringify_request (char *buf, size_t n, const DurableSupport_request *request, bool valid_data)
{
  size_t i = 0;
  int l;
  int64_t sec;
  uint32_t msec;
  char id_str[37];

  if (buf == NULL) {
    goto err;
  }
  if (request == NULL) {
    buf[0] = '\0';
    return 0;
  }
  buf[i++] = '{';
  if ((l = dc_stringify_request_key(buf+i, n-i, &request->key)) < 0) {
    goto err;
  }
  i += (size_t)l;
  if (i >= n) {
    goto trunc;
  }
  /* print non-key fields for valid data */
  if (valid_data) {
    if ((l = snprintf(buf+i, n-i, ", \"client\":\"%s\"", dc_stringify_id(request->client, id_str))) < 0) {
      goto err;
    }
    i += (size_t)l;
    if (i >= n) {
      goto trunc;
    }
    if (request->timeout == DDS_INFINITY) {
      if ((l = snprintf(buf+i, n-i, ", \"timeout\":\"never\"")) < 0) {
        goto err;
      }
    } else {
      sec = (int64_t)(request->timeout / DDS_NSECS_IN_SEC);
      msec = (uint32_t)((request->timeout % DDS_NSECS_IN_SEC) / DDS_NSECS_IN_MSEC);
      if ((l = snprintf(buf+i, n-i, ", \"timeout\":%" PRId64 ".%03" PRIu32, sec, msec)) < 0) {
        goto err;
      }
    }
    i += (size_t)l;
    if (i >= n) {
      goto trunc;
    }
  }
  if (i < n) {
    buf[i++] = '}';
  }
  if (i < n) {
    buf[i] = '\0';
  }
trunc:
  if (i >= n) {
    /* truncated */
    buf[n-1] = '\0';
  }
  return (int)i;
err:
  DDS_ERROR("dc_stringify_request failed");
  return -1;
}

static int dc_stringify_response_set (char *buf, size_t n, const DurableSupport_response_set_t *response_set)
{
  size_t i = 0, j;
  int l;
  char id_str[37];
  bool first = true;

  if (buf == NULL) {
    goto err;
  }
  if (response_set == NULL) {
    buf[0] = '\0';
    return 0;
  }
  if ((l = snprintf(buf+i, n-i, "{\"delivery_id\":%" PRIu64 ", \"partition\":\"%s\", \"tpname\":\"%s\", \"type_id\":\"%s\", \"flags\":\"0x%04" PRIx32 "\", \"guids\":[", response_set->delivery_id, response_set->partition, response_set->tpname, response_set->type_id, response_set->flags)) < 0) {
    goto err;
  }
  i += (size_t)l;
  if (i >= n) {
    goto trunc;
  }
  for(j=0; j < response_set->guids._length; j++) {
    if ((l = snprintf(buf+i, n-i, "%s\"%s\"", (first) ? "" : ",", dc_stringify_id(response_set->guids._buffer[j], id_str))) < 0) {
      goto err;
    }
    i += (size_t)l;
    if (i >= n) {
      goto trunc;
    }
    first = false;
  }
  if (i < n) {
    buf[i++] = ']';
  }
  if (i < n) {
    buf[i++] = '}';
  }
trunc:
  if (i >= n) {
    /* truncated */
    buf[n-1] = '\0';
  }
  return (int)i;
err:
  DDS_ERROR("dc_stringify_response_set failed");
  return -1;
}

static int dc_stringify_response_reader (char *buf, size_t n, const DurableSupport_response_reader_t *response_reader)
{
  size_t i = 0;
  int l;
  char id_str[37];

  if (buf == NULL) {
    goto err;
  }
  if (response_reader == NULL) {
    buf[0] = '\0';
    return 0;
  }
  if ((l = snprintf(buf+i, n-i, "{\"rguid\":\"%s\"}" , dc_stringify_id(response_reader->rguid, id_str))) < 0) {
    goto err;
  }
  i += (size_t)l;
  if (i >= n) {
    /* truncated */
    buf[n-1] = '\0';
  }
  return (int)i;
err:
  DDS_ERROR("dc_stringify_response_reader failed");
  return -1;
}

static int dc_stringify_response_data (char *buf, size_t n, const DurableSupport_response_data_t *response_data)
{
  size_t i = 0;
  int l;
  char blob_str[64];

  if (buf == NULL) {
    goto err;
  }
  if (response_data == NULL) {
    buf[0] = '\0';
    return 0;
  }
  if (dc_blob_image(response_data->blob, blob_str, sizeof(blob_str)) < 0) {
    goto err;
  }
  if ((l = snprintf(buf+i, n-i, "{\"blob\":\"%s\"", blob_str)) < 0) {
    goto err;
  }
  i += (size_t)l;
  if (i >= n) {
    goto trunc;
  }
  if (i < n) {
    buf[i++] = '}';
  }
trunc:
  if (i >= n) {
    /* truncated */
    buf[n-1] = '\0';
  }
  return (int)i;
err:
  DDS_ERROR("dc_stringify_response_data failed");
  return -1;
}

static int dc_stringify_response (char *buf, size_t n, const DurableSupport_response *response)
{
  size_t i = 0;
  int l;
  char id_str[37];

  if (buf == NULL) {
    goto err;
  }
  if (response == NULL) {
    buf[0] = '\0';
    return 0;
  }
  if ((l = snprintf(buf+i, n-i, "{\"id\":\"%s\", \"type\":\"%s\", \"content\":", dc_stringify_id(response->id, id_str), dc_responsetype_image(response->body._d))) < 0) {
    goto err;
  }
  i += (size_t)l;
  if (i >= n) {
    goto trunc;
  }
  switch (response->body._d) {
    case DurableSupport_RESPONSETYPE_SET :
      if ((l = dc_stringify_response_set(buf+i, n-i, &response->body._u.set)) < 0) {
        goto err;
      }
      i += (size_t)l;
      break;
    case DurableSupport_RESPONSETYPE_READER :
      if ((l = dc_stringify_response_reader(buf+i, n-i, &response->body._u.reader)) < 0) {
        goto err;
      }
      i += (size_t)l;
      break;
    case DurableSupport_RESPONSETYPE_DATA :
      if ((l = dc_stringify_response_data(buf+i, n-i, &response->body._u.data)) < 0) {
        goto err;
      }
      i += (size_t)l;
      break;
    default:
      goto err;
  }
  if (i < n) {
    buf[i++] = '}';
  }
trunc:
  if (i >= n) {
    /* truncated */
    buf[n-1] = '\0';
  }
  return (int)i;
err:
  DDS_ERROR("dc_stringify_response failed");
  return -1;
}

/****** end of common functions *******/

struct server_t {
  ddsrt_avl_node_t node;
  DurableSupport_id_t id; /* id key */
  char id_str[37]; /* cached string representation of the id */
  char *hostname;  /* advertised hostname */
  char *name;  /* human readable name */
};

static int cmp_server (const void *a, const void *b)
{
  return memcmp(a, b, 16);
}

static void cleanup_server (void *n)
{
  struct server_t *server = (struct server_t *)n;
  ddsrt_free(server->hostname);
  ddsrt_free(server->name);
  ddsrt_free(server);
}

static const ddsrt_avl_ctreedef_t server_td = DDSRT_AVL_CTREEDEF_INITIALIZER(offsetof (struct server_t, node), offsetof (struct server_t, id), cmp_server, 0);

struct delivery_reader_key_t {
  dds_guid_t rguid;
};

struct delivery_reader_t {
  ddsrt_avl_node_t node;  /* represents a node in the tree of readers for which there is a delivery pending */
  struct delivery_reader_key_t key;  /* key of a delivery reader */
};

static int cmp_delivery_reader (const void *a, const void *b)
{
  struct delivery_reader_key_t *k1 = (struct delivery_reader_key_t *)a;
  struct delivery_reader_key_t *k2 = (struct delivery_reader_key_t *)b;

  return memcmp(k1->rguid.v, k2->rguid.v, 16);
}

static void cleanup_delivery_reader (void *n)
{
  struct delivery_reader_t *dr = (struct delivery_reader_t *)n;
  ddsrt_free(dr);
}

static const ddsrt_avl_ctreedef_t delivery_readers_td = DDSRT_AVL_CTREEDEF_INITIALIZER(offsetof (struct delivery_reader_t, node), offsetof (struct delivery_reader_t, key), cmp_delivery_reader, 0);

struct delivery_request_key_t {
  dds_guid_t guid; /* the guid of the reader that requested historical data */
};

/* This represents a request for historical data from a client to a ds.
 * Such delivery request will lead to the publication of a transient-local
 * autodisposed request topic to a DS. A delivery requests is keyed by the
 * guid of the reader guid that request the historical data.
 * Delivery requests carry an expiration time. When the expiration time
 * expires, the delivery request will be disposed and is not available
 * any more for late joining ds's.
 *
 * LH: to check: is it really necessary to allow expiration of requests?
 * If a request is transient-local, autodisposed then there is no need
 * to expire a request. Only if you want to request data from a different
 * ds we should dispose the current requests, create another request reader
 * that connects to another ds, and republish the requests. */

struct delivery_request_t {
  ddsrt_avl_node_t node;  /* represents a node in the tree of delivery requests */
  ddsrt_fibheap_node_t fhnode;  /* represents a node in the priority queue */
  struct delivery_request_key_t key;  /* key of the delivery request; represents the reader that requested historical data */
  dds_entity_t reader; /* the reader entity that requested historical data */
  dds_time_t exp_time;  /* delivery request expiration time */
};

static int dc_stringify_delivery_request_key (char *buf, size_t n, const struct delivery_request_key_t *key)
{
  size_t i = 0;
  int l;
  char guid_str[37];

  if (buf == NULL) {
    goto err;
  }
  if (key == NULL) {
    buf[0] = '\0';
    return 0;
  }
  if ((l = snprintf(buf+i, n-i, "\"key\":{\"guid\":\"%s\"}", dc_stringify_id(key->guid.v, guid_str))) < 0) {
    goto err;
  }
  i += (size_t)l;
  if (i >= n) {
    /* truncated */
    buf[n-1] = '\0';
  }
  return (int)i;
err:
  DDS_ERROR("dc_stringify_delivery_request_key failed");
  return -1;
}

static int dc_stringify_delivery_request (char *buf, size_t n, const struct delivery_request_t *dr)
{
  size_t i = 0;
  int l;
  int64_t sec;
  uint32_t msec;

  if (buf == NULL) {
    goto err;
  }
  if (dr == NULL) {
    buf[0] = '\0';
    return 0;
  }
  buf[i++] = '{';
  if ((l = dc_stringify_delivery_request_key(buf+i, n-i, &dr->key)) < 0) {
    goto err;
  }
  i += (size_t)l;
  if (i >= n) {
    goto trunc;
  }
  if ((l = snprintf(buf+i, n-i, ", \"reader\":%" PRId32, dr->reader)) < 0) {
    goto err;
  }
  i += (size_t)l;
  if (i >= n) {
    goto trunc;
  }
  if (dr->exp_time == DDS_INFINITY) {
    if ((l = snprintf(buf+i, n-i, ", \"exp_time\":\"never\"")) < 0) {
      goto err;
    }
  } else {
    sec = (int64_t)(dr->exp_time / DDS_NSECS_IN_SEC);
    msec = (uint32_t)((dr->exp_time % DDS_NSECS_IN_SEC) / DDS_NSECS_IN_MSEC);
    if ((l = snprintf(buf+i, n-i, ", \"exp_time\":%" PRId64 ".%03" PRIu32, sec, msec)) < 0) {
      goto err;
    }
  }
  i += (size_t)l;
  if (i >= n) {
    goto trunc;
  }
  buf[i++]='}';
  if (i < n) {
    buf[i] = '\0';
  }
trunc:
  if (i >= n) {
    /* truncated */
    buf[n-1] = '\0';
  }
  return (int)i;
err:
  DDS_ERROR("dc_stringify_delivery_request failed");
  return -1;
}

static int cmp_delivery_request (const void *a, const void *b)
{
  struct delivery_request_key_t *k1 = (struct delivery_request_key_t *)a;
  struct delivery_request_key_t *k2 = (struct delivery_request_key_t *)b;

  return memcmp(k1->guid.v, k2->guid.v, 16);
}

static void cleanup_delivery_request (void *n)
{
  struct delivery_request_t *dr = (struct delivery_request_t *)n;
  ddsrt_free(dr);
}

static int cmp_exp_time (const void *a, const void *b)
{
  struct delivery_request_t *dr1 = (struct delivery_request_t *)a;
  struct delivery_request_t *dr2 = (struct delivery_request_t *)b;

  return (dr1->exp_time < dr2->exp_time) ? -1 : ((dr1->exp_time > dr2->exp_time) ? 1 : 0);
}

static const ddsrt_avl_ctreedef_t delivery_requests_td = DDSRT_AVL_CTREEDEF_INITIALIZER(offsetof (struct delivery_request_t, node), offsetof (struct delivery_request_t, key), cmp_delivery_request, 0);

static const ddsrt_fibheap_def_t delivery_requests_fd = DDSRT_FIBHEAPDEF_INITIALIZER(offsetof (struct delivery_request_t, fhnode), cmp_exp_time);

struct delivery_ctx_t {
  ddsrt_avl_node_t node;
  DurableSupport_id_t id; /* id of the ds that delivers the data; key of the delivery context */
  uint64_t delivery_id;  /* the id of the delivery by this ds; this is a monotonic increased sequence number */
  ddsrt_avl_ctree_t readers; /* tree of reader guids for which this delivery is intended; populated when a delivery is opened */
};

static int dc_stringify_delivery_ctx (char *buf, size_t n, const struct delivery_ctx_t *delivery_ctx)
{
  size_t i = 0;
  int l;
  char id_str[37];
  struct delivery_reader_t *delivery_reader;
  ddsrt_avl_citer_t it;
  bool first = true;

  if (buf == NULL) {
    goto err;
  }
  if (delivery_ctx == NULL) {
    buf[0] = '\0';
    return 0;
  }
  if ((l = snprintf(buf+i, n-i, "{\"id\":\"%s\", \"readers\":[", dc_stringify_id(delivery_ctx->id, id_str))) < 0) {
    goto err;
  }
  i += (size_t)l;
  if (i >= n) {
    goto trunc;
  }
  for (delivery_reader = ddsrt_avl_citer_first (&delivery_readers_td, &delivery_ctx->readers, &it); delivery_reader; delivery_reader = ddsrt_avl_citer_next (&it)) {
    if ((l = snprintf(buf+i, n-i, "%s%s", (first) ? "" : ",", dc_stringify_id(delivery_reader->key.rguid.v, id_str))) < 0) {
      goto err;
    }
    i += (size_t)l;
    if (i >= n) {
      goto trunc;
    }
    first = false;
  }
  if ((l = snprintf(buf+i, n-i, "]}")) < 0) {
    goto err;
  }
  i += (size_t)l;
trunc:
  if (i >= n) {
    /* truncated */
    buf[n-1] = '\0';
  }
  return (int)i;
err:
  DDS_ERROR("dc_stringify_delivery_ctx failed");
  return -1;
}

static int delivery_ctx_cmp (const void *a, const void *b)
{
  return  memcmp(a, b, 16);
}

static void cleanup_delivery_ctx (void *n)
{
  struct delivery_ctx_t *delivery_ctx = (struct delivery_ctx_t *)n;
  ddsrt_avl_cfree(&delivery_readers_td,  &delivery_ctx->readers, cleanup_delivery_reader);
  ddsrt_free(delivery_ctx);
}

static const ddsrt_avl_ctreedef_t delivery_ctx_td = DDSRT_AVL_CTREEDEF_INITIALIZER(offsetof (struct delivery_ctx_t, node), offsetof (struct delivery_ctx_t, id), delivery_ctx_cmp, 0);

/* Administration to keep track of the request readers of a DS
 * that can act as a target to send requests for historical data to..
 * Based on this administration requests for historical data are published immediately,
 * or pending until a ds is found that matches. */

struct matched_request_reader_t {
  ddsrt_avl_node_t node;
  dds_instance_handle_t ih;  /* the instance handle of the matched request reader for my own request writer */
};

static int cmp_matched_request_reader (const void *a, const void *b)
{
  dds_instance_handle_t *ih1 = (dds_instance_handle_t *)a;
  dds_instance_handle_t *ih2 = (dds_instance_handle_t *)b;

  return (*ih1 < *ih2) ? -1 : ((*ih1 > *ih2) ? 1 : 0);
}

static void cleanup_matched_request_reader (void *n)
{
  struct matched_request_reader_t *mrr = (struct matched_request_reader_t *)n;
  ddsrt_free(mrr);
}

static const ddsrt_avl_ctreedef_t matched_request_readers_td = DDSRT_AVL_CTREEDEF_INITIALIZER(offsetof (struct matched_request_reader_t, node), offsetof (struct matched_request_reader_t, ih), cmp_matched_request_reader, 0);

struct com_t {
  dds_entity_t participant; /* durable client participant */
  dds_entity_t status_subscriber; /* subscriber used to receive status messages */
  dds_entity_t request_publisher;  /* publisher to send requests */
  dds_entity_t response_subscriber; /* subscriber used to receive response messages */
  dds_entity_t tp_status;  /* status topic */
  dds_entity_t rd_status;  /* status reader */
  dds_entity_t rc_status;  /* status read condition */
  dds_entity_t tp_request; /* request topic */
  dds_entity_t wr_request; /* request writer */
  dds_entity_t tp_response; /* response topic */
  dds_entity_t rd_response; /* response reader */
  dds_entity_t rc_response; /* response read condition */
  dds_entity_t rd_participant; /* participant reader */
  dds_entity_t rd_subinfo; /* DCPSSubscription reader */
  dds_entity_t rc_subinfo; /* DCPSSubscription read condition */
  dds_entity_t delivery_request_guard;  /* trigger expiration of delivery request */
  dds_listener_t *status_listener; /* listener on status topic */
  dds_entity_t ws;
};

/* This struct contains the main durable client administration.
 * This struct is initialized when an application creates the first participant, and
 * deinitialized when the last participant is deleted.
 * In case an application tries to create multiple participants at once (e.q.,
 * using different threads), we only want to initialize the durable client
 * administration only once. For that purpose, we keep an atomic refcount to
 * that tracks how many participants are created.
 * we only want a
 */
struct dc_t {
  struct {
    DurableSupport_id_t id; /* the id of this client */
    char id_str[37]; /* string representation of the client id */
    char *request_partition;  /* partition to send requests too; by default same as <hostname> */
    uint32_t quorum;  /*quorum of durable services needed to unblock durable writers */
    char *ident; /* ds identification */
  } cfg;
  uint64_t seq;  /* monotonically increasing sequence number, bumped by 1 for each request by this client */
  ddsrt_atomic_uint32_t refcount;  /* refcount, increased/decreased when a participant is created/deleted */
  struct ddsi_domaingv *gv;  /* reference to ddsi domain settings */
  struct com_t *com;  /* ptr to durable client communication infra structure */
  bool quorum_reached;
  ddsrt_thread_t recv_tid;  /* receiver thread */
  ddsrt_threadattr_t recv_tattr; /* receiver thread attributes */
  ddsrt_mutex_t recv_mutex; /* recv mutex */
  ddsrt_cond_t recv_cond; /* recv condition */
  ddsrt_atomic_uint32_t termflag;  /* termination flag, initialized to 0 */
  ddsrt_avl_ctree_t servers; /* tree containing all discovered durable servers */
  dds_listener_t *subinfo_listener;  /* listener to detect remote containers */
  dds_listener_t *quorum_listener; /* listener to check if a quorum is reached */
  dds_listener_t *request_listener; /* listener to check if request reader is available */
  ddsrt_avl_ctree_t delivery_requests; /* tree containing delivery requests  */
  ddsrt_fibheap_t delivery_requests_fh; /* priority queue for delivery requests, prioritized by expiry time */
  ddsrt_mutex_t delivery_request_mutex; /* delivery request queue mutex */
  ddsrt_cond_t delivery_request_cond; /* delivery request condition */
  dds_instance_handle_t selected_request_reader_ih; /* instance handle to matched request reader, DDS_HANDLE_NIL if not available */
  ddsrt_avl_ctree_t matched_request_readers; /* tree containing the request readers on DSs that match with my request writer */
  uint32_t nr_of_matched_dc_requests; /* indicates the number of matched dc_request readers for the dc_request writer of this client */
  struct delivery_ctx_t *delivery_ctx;  /* reference to current delivery context, NULL if none */
  ddsrt_avl_ctree_t delivery_ctxs; /* table of delivery contexts, keyed by ds id */
};

static struct dc_t dc = { 0 };  /* static durable client structure */

static unsigned split_string (const char ***p_ps, char **p_bufcopy, const char *buf, const char delimiter)
{
  const char *b;
  const char **ps;
  char *bufcopy, *bc;
  unsigned i, nps;
  nps = 1;
  for (b = buf; *b; b++) {
    nps += (*b == delimiter);
  }
  ps = dds_alloc(nps * sizeof(*ps));
  bufcopy = ddsrt_strdup(buf);
  i = 0;
  bc = bufcopy;
  while (1) {
    ps[i++] = bc;
    while (*bc && *bc != delimiter) bc++;
    if (*bc == 0) break;
    *bc++ = 0;
  }
  assert(i == nps);
  *p_ps = ps;
  *p_bufcopy = bufcopy;
  return nps;
}

static struct server_t *create_server (struct dc_t *dc, DurableSupport_id_t id, const char *name, const char *hostname)
{
  struct server_t *server;
  char id_str[37]; /* guid */

  assert(name);
  assert(hostname);
  /* the ds is not known yet by the dc, let's create it */
  if ((server = (struct server_t *)ddsrt_malloc(sizeof(struct server_t))) == NULL) {
    goto err_alloc_server;
  }
  memcpy(server->id, id, 16);
  dc_stringify_id(server->id, server->id_str);
  server->name = ddsrt_strdup(name);
  server->hostname = ddsrt_strdup(hostname);
  ddsrt_avl_cinsert(&server_td, &dc->servers, server);
  return server;

err_alloc_server:
  DDS_ERROR("Failed to create ds for id \"%s\"\n", dc_stringify_id(id, id_str));
  return NULL;
}

static int get_host_specific_partition_name (char *buf, size_t len)
{
   char hostname[256];
   dds_return_t ret;
   int l;

   if ((len == 0) || (buf == NULL)) {
     DDS_ERROR("No storage for hostname available, unable to determine the host specific partition\n");
     return -1;
   }
   if ((ret = ddsrt_gethostname(hostname, 256)) < 0) {
     DDS_ERROR("Hostname limit of 256 exceeded, unable to determine the host specific partition [%s]\n", dds_strretcode(ret));
     return -1;
   }
   if ((l = snprintf(buf, len, "%s", hostname)) < 0) {
     DDS_ERROR("Failed to construct the host specific partition name [%s]\n", dds_strretcode(ret));
     return -1;
   }
   if (len <= (size_t)l) {
     DDS_ERROR("Host specific partition name '%s' too long\n", buf);
     return -1;
   }
   return 0;
}

/* add a participant specific partition to the configured partition for client requests */
/* create a comma separated list consisting of the 'hostname' followed by the
 * list configured by cfg->request_partition.
 * The combined length may not exceed 1024 characters (including the '\0' terminator)  */
static int create_request_partition_expression (struct com_t *com, char **request_partition)
{
  char req_pname[1024] = { 0 };
  int result = 0;

  (void)com;
  if ((result = get_host_specific_partition_name(req_pname, 1024)) < 0) {
    DDS_ERROR("Failed to create request partition expression\n");
    return -1;
  }
  /* todo: LH perhaps add a configurable set of request partitions */
  /* set the request partition*/
  *request_partition = ddsrt_strdup(req_pname);
  return 0;
}

/* The following is used to build up an administration to evaluate
 * if a writer has reached its quorum for durable containers.
 * The administration uses data container counters to count the
 * number of data container matches for a given writer. If the writer
 * publishes on multiple partitions, we requires that for each of
 * these partitions the quorum must be met in order for the publisher
 * to start publishing.
 * To retrieve the matching data containers for a writer we use
 * dds_get_matched_subscriptions(). This call returns the matched
 * reader for a given writer, even the onces that occurred before the
 * quorum listener has been attached to the writer. To reduce the
 * number of calls to dds_get_matched_subscriptions() we only call
 * it there is a risk that the quorum changes.
 */
struct data_container_cnt_key_t {
  char *partition;
};

struct data_container_cnt_t {
  ddsrt_avl_node_t node;
  struct data_container_cnt_key_t key;
  uint32_t cnt;
};

static void cleanup_data_container_cnt (void *n)
{
  struct data_container_cnt_t *dcc = (struct data_container_cnt_t *)n;

  ddsrt_free(dcc->key.partition);
  ddsrt_free(dcc);
}

static int cmp_data_container_cnt (const void *a, const void *b)
{
  struct data_container_cnt_key_t *k1 = (struct data_container_cnt_key_t *)a;
  struct data_container_cnt_key_t *k2 = (struct data_container_cnt_key_t *)b;

  return strcmp(k1->partition, k2->partition);
}

static const ddsrt_avl_ctreedef_t data_container_cnt_td = DDSRT_AVL_CTREEDEF_INITIALIZER(offsetof (struct data_container_cnt_t, node), offsetof (struct data_container_cnt_t, key), cmp_data_container_cnt, 0);


static struct data_container_cnt_t *create_data_container_cnt (ddsrt_avl_ctree_t *dcc_tree, const char *partition)
{
  struct data_container_cnt_t *dcc;

  assert(partition);
  dcc = (struct data_container_cnt_t *)ddsrt_malloc(sizeof(struct data_container_cnt_t));
  dcc->key.partition = ddsrt_strdup(partition);
  dcc->cnt = 0;
  ddsrt_avl_cinsert(&data_container_cnt_td, dcc_tree, dcc);
  return dcc;
}

static struct data_container_cnt_t *get_data_container_cnt (ddsrt_avl_ctree_t *dcc_tree, const char *partition, bool autocreate)
{
  struct data_container_cnt_t *dcc = NULL;
  struct data_container_cnt_key_t key;

  assert(dcc_tree);
  assert(partition);
  key.partition = ddsrt_strdup(partition);
  if (((dcc = ddsrt_avl_clookup (&data_container_cnt_td, dcc_tree, &key)) == NULL) && autocreate) {
    dcc = create_data_container_cnt(dcc_tree, partition);
  }
  ddsrt_free(key.partition);
  return dcc;
}

static void dc_free_partitions (uint32_t plen, char **partitions)
{
  uint32_t i;

  if (partitions == NULL) {
    return;
  }
  for (i=0; i < plen; i++) {
    ddsrt_free(partitions[i]);
  }
  ddsrt_free(partitions);
}

/* verifies if the user data of the endpoint contains the identifier
 * that indicates that this endpoint is a durable container */
static bool dc_is_ds_endpoint (struct com_t *com, dds_builtintopic_endpoint_t *ep, const char *ident)
{
  dds_builtintopic_endpoint_t template;
  dds_builtintopic_participant_t *participant;
  dds_instance_handle_t ih;
  dds_return_t rc;
  void *samples[1];
  dds_sample_info_t info[1];
  void *userdata = NULL;
  size_t size = 0;
  char id_str[37];
  bool result = false;
  samples[0] = NULL;

  assert(ep);
  /* by convention, if the ident == NULL then return true */
  if (ident == NULL) {
    return true;
  }
  /* lookup the instance handle of the builtin participant endpoint that
   * contains the participant of the subinfo */
  memcpy(template.key.v,ep->participant_key.v,16);
  if ((ih = dds_lookup_instance(com->rd_participant, &template)) == DDS_HANDLE_NIL) {
    DDS_ERROR("Failed to lookup the participant of reader \"%s\"", dc_stringify_id(ep->key.v, id_str));
    goto err_lookup_instance;
  }
  if ((rc = dds_read_instance(com->rd_participant, samples, info, 1, 1, ih)) <= 0) {
    DDS_ERROR("Failed to read the participant of reader \"%s\"", dc_stringify_id(ep->key.v, id_str));
    goto err_read_instance;
  }
  if (info[0].valid_data) {
    participant = (dds_builtintopic_participant_t *)samples[0];
    /* get the user data */
    if (!dds_qget_userdata(participant->qos, &userdata, &size)) {
      DDS_ERROR("Unable to retrieve the user data of reader \"%s\"", dc_stringify_id(ep->key.v, id_str));
      goto err_qget_userdata;
    }
    if ((size != strlen(ident)) || (userdata == NULL) || (strcmp(userdata, ident) != 0)) {
      /* the user data of the participant of the durable reader does not contain the ident,
       * so the endoint is not from a remote DS */
      result = false;
    } else {
      /* this endpoint's participant is a ds */
      result = true;
    }
    dds_free(userdata);
  }
  (void)dds_return_loan(com->rd_participant, samples, rc);
  return result;

err_qget_userdata:
  (void)dds_return_loan(com->rd_participant, samples, rc);
err_read_instance:
err_lookup_instance:
  return false;
}

/* Determine if the quorum for the writer is reached or not.
 * This function also does its job when the quorum threshold is 0,
 * even though this case should likely be prohibited because it
 * violates eventual consistency. After all, how can you provide
 * historical data if you allow that durable publishers start to
 * publish when there is no durable support? */
static void dc_check_quorum_reached (struct dc_t *dc, dds_entity_t writer, bool wr_appeared)
{
  /* precondition: the writer is only called when we know that the writer is durable */

  dds_qos_t *qos;
  dds_return_t ret;
  uint32_t plen, i;
  char **partitions;
  char *tpname;
  dds_writer *wr;
  bool old_quorum_reached, quorum_reached = true;
  bool to_check = false;
  struct data_container_cnt_t *dcc;
  dds_guid_t wguid;
  char id_str[37];

  qos = dds_create_qos();
  if ((ret = dds_get_qos(writer, qos)) < 0) {
    DDS_ERROR("failed to get qos from writer [%s]\n", dds_strretcode(ret));
    goto err_get_qos;
  }
  if (!dds_qget_partition(qos, &plen, &partitions)) {
    DDS_ERROR("failed to get partitions from qos\n");
    goto err_qget_partition;
  }
  assert(plen > 0);
  if (dds_get_guid(writer, &wguid) < 0) {
    DDS_ERROR("failed to writer guid\n");
    goto err_get_guid;
  }
  /* determine if the quorum is already satisfied or not */
  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK) {
    DDS_ERROR("failed to lock writer [%s]\n", dds_strretcode(ret));
    goto err_writer_lock;
  }
  tpname = ddsrt_strdup(wr->m_topic->m_name);
  old_quorum_reached = wr->quorum_reached;
  to_check = ((!wr->quorum_reached) && (wr_appeared)) || ((wr->quorum_reached) && (!wr_appeared));
  dds_writer_unlock (wr);
  if (to_check) {
    dds_instance_handle_t *rd_ihs;
    size_t nrds = 128;
    ddsrt_avl_ctree_t data_container_counters;

    DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "checking quorum for writer \"%s\"\n", dc_stringify_id(wguid.v, id_str));
    ddsrt_avl_cinit(&data_container_cnt_td, &data_container_counters);
    /* Check if the quorum for the writer is lost or reached.
     * We do this by calling dds_get_matched_subscriptions().
     * This is an expensive call, but it works for now. At least we
     * are protected against missing publication_matches() that could have
     * occurred before the quorum listener was set on the writer.
     * We only do this expensive call when there is a possibility
     * that the quorum_reached property of the writer changes.
     * Because the call to dds_get_matched_subscriptions() requires
     * an preallocated list of reader handles and we don't know the
     * size of the list beforehand, we initially bound the list to
     * 256 and extend it dynamically  until it is large enough to hold
     * all handles of matching readers. */
    do {
      nrds = nrds * 2;
      rd_ihs = ddsrt_malloc(nrds * sizeof(dds_instance_handle_t));
      if ((ret = dds_get_matched_subscriptions(writer, rd_ihs, nrds)) > (int32_t)nrds) {
        /* allocated list is too small, use a bigger list */
        ddsrt_free(rd_ihs);
      }
    } while (ret > (int32_t)nrds);
    /* We now have a list of handles to readers that match with the writer.
     * Determine if the quorum is reached by verifying if the reader
     * is a data container, an counting the matches. */
    if ((ret >= 0) && (dc->cfg.quorum > (uint32_t)ret)) {
      /* the number of matches is less than the quorum, so we are sure that
       * the quorum cannot be reached.  */
      quorum_reached = false;
    } else if (ret >= 0) {
      /* now walk over all reader handles, lookup the reader,
       * and determine if the reader is a remote data container.
       * If so, we have detected a matching remote data container and
       * we increase the count for this container. */
      for (i=0; i < (uint32_t)ret; i++) {
        dds_builtintopic_endpoint_t *ep;

        if ((ep = dds_get_matched_subscription_data(writer, rd_ihs[i])) != NULL) {
          if (dc_is_ds_endpoint(dc->com, ep, dc->cfg.ident)) {
            /* the matching endpoint represents a data container.
             * Increase the count for this container. */
            uint32_t ep_plen;
            char **ep_partitions;

            dds_qget_partition(ep->qos, &ep_plen, &ep_partitions);
            assert(ep_plen == 1); /* this is a data container, so it must have a singleton as partition */
            dcc = get_data_container_cnt(&data_container_counters, ep_partitions[0], true);
            dcc->cnt++;
            dc_free_partitions(ep_plen, ep_partitions);
          }
          dds_builtintopic_free_endpoint(ep);
        }
      }
      /* Determine if the quorum is reached or not.
       * We do this by walking over the partitions of the writer,
       * lookup the corresponding data container counter, and determine
       * if they meet the quorum. Only if the quorum is met for all
       * partitions of the writer, then we are sure that the writer meets
       * the quorum condition, and publication can proceed. */
      quorum_reached = true;
      for (i=0; i < plen; i++) {
        if ((dcc = get_data_container_cnt(&data_container_counters, partitions[i], false)) == NULL) {
          /* no data container counter found for this partitions of the writer, so
           * quorom not reached */
          quorum_reached = false;
          break;
        } else if (dcc->cnt < dc->cfg.quorum) {
          /* quorom not (yet) reached */
          quorum_reached = false;
          break;
        }
      }
    }
    if (old_quorum_reached != quorum_reached) {
      /* the quorum has changed, update the writer quorum setting */
      if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK) {
        DDS_ERROR("failed to lock writer [%s]\n", dds_strretcode(ret));
        goto err_writer_lock;
      }
      wr->quorum_reached = quorum_reached;
      dds_writer_unlock (wr);
      DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "quorum for writer \"%s\" %s\n", dc_stringify_id(wguid.v, id_str), quorum_reached ? "reached" : "lost");
    } else {
      DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "quorum for writer \"%s\" still %sreached\n", dc_stringify_id(wguid.v, id_str), quorum_reached ? "" : "not ");
    }
    ddsrt_avl_cfree(&data_container_cnt_td, &data_container_counters, cleanup_data_container_cnt);
    ddsrt_free(rd_ihs);
  }
  dc_free_partitions(plen, partitions);
  ddsrt_free(tpname);
  dds_delete_qos(qos);
  return;

err_writer_lock:
err_get_guid:
  dc_free_partitions(plen, partitions);
err_qget_partition:
err_get_qos:
  dds_delete_qos(qos);
  return;
}

/* set up durable client infrastructure */
static struct com_t *dc_com_new (struct dc_t *dc, const dds_domainid_t domainid, struct ddsi_domaingv *gv)
{
  struct com_t *com;
  dds_qos_t *tqos = NULL;
  dds_qos_t *status_sqos = NULL, *status_rqos = NULL;
  dds_qos_t *request_pqos = NULL, *request_wqos = NULL;
  dds_qos_t *response_sqos = NULL, *response_rqos = NULL;
  const char **ps1, **ps2;
  char *bufcopy1, *bufcopy2;
  unsigned nps;
  dds_return_t ret;
  char *request_partition = NULL;
  dds_guid_t guid;

  (void)dc;
  if ((com = (struct com_t *)ddsrt_malloc(sizeof(struct com_t))) == NULL) {
    DDS_ERROR("failed to allocate dc communication infrastructure\n");
    goto err_alloc_com;
  }
  /* create participant, subscriber and publisher for durable client support */
  if ((com->participant = dds_create_participant(domainid, NULL, NULL)) < 0) {
    DDS_ERROR("failed to create dc participant [%s]\n", dds_strretcode(-com->participant));
    goto err_create_participant;
  }
  /* use the participant guid as the identification of this client */
  if ((ret = dds_get_guid(com->participant, &guid)) != DDS_RETCODE_OK) {
    DDS_ERROR("failed to get dc participant guid [%s]\n", dds_strretcode(-ret));
    goto err_get_guid;
  }
  /* get and cache the id of the participant */
  memcpy(dc->cfg.id, guid.v, 16);
  (void)dc_stringify_id(dc->cfg.id, dc->cfg.id_str);
  /* create subscriber */
  if ((status_sqos = dds_create_qos()) == NULL) {
    DDS_ERROR("failed to create the dc status subscriber qos\n");
    goto err_alloc_status_sqos;
  }
  /* todo: LH make the partition used to receive status message configurable */
  nps = split_string(&ps1, &bufcopy1, "durable_support", ',');
  dds_qset_partition (status_sqos, nps, ps1);
  if ((com->status_subscriber = dds_create_subscriber(com->participant, status_sqos, NULL)) < 0) {
    DDS_ERROR("failed to create dc status subscriber [%s]\n", dds_strretcode(-com->status_subscriber));
    goto err_status_subscriber;
  }
  /* create status reader */
  if ((tqos = dds_create_qos()) == NULL) {
    DDS_ERROR("failed to create the dc status topic qos\n");
    goto err_alloc_tqos;
  }
  dds_qset_durability(tqos, DDS_DURABILITY_TRANSIENT_LOCAL);
  dds_qset_reliability(tqos, DDS_RELIABILITY_RELIABLE, DDS_SECS (1));
  dds_qset_destination_order(tqos, DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);
  dds_qset_history(tqos, DDS_HISTORY_KEEP_LAST, 1);
  dds_qset_ignorelocal (tqos, DDS_IGNORELOCAL_PARTICIPANT);
  if ((com->tp_status = dds_create_topic (com->participant, &DurableSupport_status_desc, "ds_status", tqos, NULL)) < 0) {
    DDS_ERROR("failed to create dc status topic [%s]\n", dds_strretcode(-com->tp_status));
    goto err_tp_status;
  }
  if ((status_rqos = dds_create_qos()) == NULL) {
    DDS_ERROR("failed to create dc status reader qos\n");
    goto err_alloc_status_rqos;
  }
  if ((ret = dds_copy_qos(status_rqos, tqos)) < DDS_RETCODE_OK) {
    DDS_ERROR("failed to copy dc status topic qos [%s]\n", dds_strretcode(-ret));
    goto err_copy_status_rqos;
  }
  if ((com->rd_status = dds_create_reader(com->status_subscriber, com->tp_status, status_rqos, NULL)) < 0) {
    DDS_ERROR("failed to create dc status reader [%s]\n", dds_strretcode(-com->rd_status));
    goto err_rd_status;
  }
  if ((com->rc_status = dds_create_readcondition (com->rd_status, DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE)) < 0) {
    DDS_ERROR("failed to create dc status read condition [%s]\n", dds_strretcode(-com->rc_status));
    goto err_rc_status;
  }
  /* create dc_request writer
   * The dc_request topic is a transient-local topic, which ensures that
   * a late joining DS can still get a request provided it has not not
   * expired. */
  if (create_request_partition_expression(com, &request_partition) < 0) {
    DDS_ERROR("failed to create dc request partition\n");
    goto err_request_partition;
  }
  nps = split_string(&ps2, &bufcopy2, request_partition, ',');
  assert(nps > 0);
  if ((request_pqos = dds_create_qos()) == NULL) {
    DDS_ERROR("failed to create dc publisher qos for request topic\n");
    goto err_alloc_request_pqos;
  }
  dds_qset_partition (request_pqos, nps, ps2);
  if ((com->request_publisher = dds_create_publisher(com->participant, request_pqos, NULL)) < 0) {
    DDS_ERROR("Failed to create dc publisher for request topic [%s]\n", dds_strretcode(-com->request_publisher));
    goto err_request_publisher;
  }
  /* create dc_request writer */
  dds_qset_durability(tqos, DDS_DURABILITY_TRANSIENT_LOCAL);
  dds_qset_history(tqos, DDS_HISTORY_KEEP_LAST, 1);
  if ((com->tp_request = dds_create_topic (com->participant, &DurableSupport_request_desc, "dc_request", tqos, NULL)) < 0) {
    DDS_ERROR("failed to create the dc request topic [%s]\n", dds_strretcode(-com->tp_request));
    goto err_tp_request;
  }
  if ((request_wqos = dds_create_qos()) == NULL) {
    DDS_ERROR("failed to create dc request writer qos\n");
    goto err_alloc_request_wqos;
  }
  if ((ret = dds_copy_qos(request_wqos, tqos)) < DDS_RETCODE_OK) {
    DDS_ERROR("failed to copy dc request topic qos [%s]\n", dds_strretcode(-ret));
    goto err_copy_request_wqos;
  }
  /* The writer data lifecycle of a dc_request writer has an
   * autodispose policy. This makes it possible to cancel requests
   * when the client disconnects from the DS. */
  dds_qset_writer_data_lifecycle(request_wqos, true);
  /* if we attach the request_listener now to the request writer here, then it
   * is that possible the function triggers before this com_new() function
   * has been finished. Because the request listener callback requires
   * the com to be initialized (in the dc_is_ds_endpoint() call) this would
   * then lead to a crash. For that reason we cannot attach the request listener
   * to wr_request here, but we have to do it after com has been initialized
   * (in activate_request_listener()). To not miss out on any triggers, we need to
   * call dds_get_matched_subscription_data() after com has been initialized */
  if ((com->wr_request = dds_create_writer(com->request_publisher, com->tp_request, request_wqos, NULL)) < 0) {
    DDS_ERROR("failed to create dc request writer [%s]\n", dds_strretcode(-com->wr_request));
    goto err_wr_request;
  }
  /* create dc_response reader */
  if ((response_sqos = dds_create_qos()) == NULL) {
    DDS_ERROR("failed to create the dc response subscriber qos qos\n");
    goto err_alloc_response_sqos;
  }
  dds_qset_partition (response_sqos, nps, ps2);
  if ((com->response_subscriber = dds_create_subscriber(com->participant, response_sqos, NULL)) < 0) {
    DDS_ERROR("failed to create dc response subscriber [%s]\n", dds_strretcode(-com->response_subscriber));
    goto err_response_subscriber;
  }
  dds_qset_durability(tqos, DDS_DURABILITY_VOLATILE);
  dds_qset_history(tqos, DDS_HISTORY_KEEP_ALL, DDS_LENGTH_UNLIMITED);
  if ((com->tp_response = dds_create_topic (com->participant, &DurableSupport_response_desc, "dc_response", tqos, NULL)) < 0) {
    DDS_ERROR("failed to create dc response topic [%s]\n", dds_strretcode(-com->tp_response));
    goto err_tp_response;
  }
  if ((response_rqos = dds_create_qos()) == NULL) {
    DDS_ERROR("failed to create dc response reader qos\n");
    goto err_alloc_response_rqos;
  }
  if ((ret = dds_copy_qos(response_rqos, tqos)) < DDS_RETCODE_OK) {
    DDS_ERROR("failed to copy dc response topic qos [%s]\n", dds_strretcode(-ret));
    goto err_copy_response_rqos;
  }
  if ((com->rd_response = dds_create_reader(com->response_subscriber, com->tp_response, response_rqos, NULL)) < 0) {
    DDS_ERROR("failed to create dc response reader [%s]\n", dds_strretcode(-com->rd_response));
    goto err_rd_response;
  }
  if ((com->rc_response = dds_create_readcondition (com->rd_response, DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE)) < 0) {
    DDS_ERROR("failed to create dc response read condition [%s]\n", dds_strretcode(-com->rc_response));
    goto err_rc_response;
  }
  /* create participant reader (to discover participants of remote durable services) */
  if ((com->rd_participant = dds_create_reader(com->participant, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, NULL)) < 0) {
    DDS_ERROR("failed to create dc participant reader [%s]\n", dds_strretcode(-com->rd_participant));
    goto err_rd_participant;
  }
  /* subinfo reader to detect remote data containers */
  if ((com->rd_subinfo = dds_create_reader(com->participant, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL)) < 0) {
    DDS_ERROR("failed to create dc subinfo reader [%s]\n", dds_strretcode(-com->rd_subinfo));
    goto err_rd_subinfo;
  }
  if ((com->rc_subinfo = dds_create_readcondition(com->rd_subinfo, DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE)) < 0) {
    DDS_ERROR("failed to create dc subinfo read condition [%s]\n", dds_strretcode(-com->rc_subinfo));
    goto err_rc_subinfo;
  }
  /* create delivery request guard condition */
  if ((com->delivery_request_guard = dds_create_guardcondition(com->participant)) < 0) {
    DDS_ERROR("failed to create delivery guard condition [%s]\n", dds_strretcode(-com->delivery_request_guard));
    goto err_create_delivery_request_guard_condition;
  }
  if ((ret = dds_set_guardcondition(com->delivery_request_guard, false)) < 0) {
    DDS_ERROR("failed to initialize delivery guard condition [%s]\n", dds_strretcode(-ret));
    goto err_set_guard_condition;
  }
  /* create waitset and attach read conditions */
  if ((com->ws = dds_create_waitset(com->participant)) < 0) {
    DDS_ERROR("failed to create dc waitset [%s]\n", dds_strretcode(-com->ws));
    goto err_waitset;
  }
  if ((ret = dds_waitset_attach (com->ws, com->delivery_request_guard, com->delivery_request_guard)) < 0) {
    DDS_ERROR("failed to attach delivery request guard condition to waitset [%s]\n", dds_strretcode(-ret));
    goto err_attach_delivery_request_guard;
  }
  if ((ret = dds_waitset_attach (com->ws, com->rc_status, com->rd_status)) < 0) {
    DDS_ERROR("failed to attach dc status reader to waitset [%s]\n", dds_strretcode(-ret));
    goto err_attach_rd_status;
  }
  if ((ret = dds_waitset_attach (com->ws, com->rc_response, com->rd_response)) < 0) {
    DDS_ERROR("failed to attach dc response reader to waitset [%s]\n", dds_strretcode(-ret));
    goto err_attach_rd_response;
  }
  if ((ret = dds_waitset_attach (com->ws, com->ws, com->ws)) < 0) {
    DDS_ERROR("failed to attach waitset to itself [%s]\n", dds_strretcode(-ret));
    goto err_attach_ws;
  }
  DDS_CLOG(DDS_LC_DUR, &gv->logconfig, "dc infrastructure created\n");
  dds_free(request_partition);
  dds_delete_qos(tqos);
  dds_delete_qos(status_sqos);
  dds_delete_qos(status_rqos);
  dds_delete_qos(request_pqos);
  dds_delete_qos(request_wqos);
  dds_delete_qos(response_sqos);
  dds_delete_qos(response_rqos);
  dds_free(bufcopy1);
  dds_free(bufcopy2);
  dds_free(ps1);
  dds_free(ps2);
  return com;

err_attach_ws:
  dds_waitset_detach(com->ws, com->rc_response);
err_attach_rd_response:
  dds_waitset_detach(com->ws, com->rc_status);
err_attach_rd_status:
  dds_waitset_detach(com->ws, com->delivery_request_guard);
err_attach_delivery_request_guard:
  dds_delete(com->ws);
err_waitset:
  dds_delete(com->delivery_request_guard);
err_create_delivery_request_guard_condition:
err_set_guard_condition:
  dds_delete(com->rc_subinfo);
err_rc_subinfo:
  dds_delete(com->rd_subinfo);
err_rd_subinfo:
  dds_delete(com->rd_participant);
err_rd_participant:
  dds_delete(com->rc_response);
err_rc_response:
  dds_delete(com->rd_response);
err_rd_response:
err_copy_response_rqos:
  dds_delete_qos(response_rqos);
err_alloc_response_rqos:
  dds_delete(com->tp_response);
err_tp_response:
  dds_delete(com->response_subscriber);
err_response_subscriber :
  dds_delete_qos(response_sqos);
err_alloc_response_sqos:
  dds_delete(com->wr_request);
err_wr_request:
err_copy_request_wqos:
  dds_delete_qos(request_wqos);
err_alloc_request_wqos:
  dds_delete(com->tp_request);
err_tp_request:
  dds_delete(com->request_publisher);
err_request_publisher:
  dds_delete_qos(request_pqos);
err_alloc_request_pqos:
  dds_free(request_partition);
  dds_free(bufcopy2);
  dds_free(ps2);
err_request_partition:
  dds_delete(com->rc_status);
err_rc_status:
  dds_delete(com->rd_status);
err_rd_status:
err_copy_status_rqos:
  dds_delete_qos(status_rqos);
err_alloc_status_rqos:
  dds_delete(com->tp_status);
err_tp_status:
  dds_delete_qos(tqos);
err_alloc_tqos:
  dds_delete(com->status_subscriber);
err_status_subscriber:
  dds_delete_qos(status_sqos);
  dds_free(bufcopy1);
  dds_free(ps1);
err_alloc_status_sqos:
err_get_guid:
  dds_delete(com->participant);
err_create_participant:
  ddsrt_free(com);
err_alloc_com:
  return NULL;
}

static void dc_com_free (struct com_t *com, dds_entity_t* pp_out)
{
  assert(com);
  DDS_CLOG(DDS_LC_DUR, &dc.gv->logconfig, "destroying dc infrastructure\n");
  if ( pp_out == NULL ){
    dds_delete(com->participant);
  } else {
    *pp_out = com->participant;
  }
  ddsrt_free(com);
  dc.com = NULL;
  return;
}

#define MAX_TOPIC_NAME_SIZE                  255

static dds_return_t dc_com_request_write (struct com_t *com, const dds_guid_t rguid)
{
  /* note: we allow doing a request for volatile readers */
  DurableSupport_request *request;
  dds_return_t ret = DDS_RETCODE_OK;
  char str[1024];
  int l;
  size_t len;

  request = DurableSupport_request__alloc();
  memcpy(request->key.rguid, rguid.v, 16);
  memcpy(request->client, dc.cfg.id, 16);
  request->timeout = DDS_INFINITY;  /* currently not used */
  l = dc_stringify_request(str, sizeof(str), request, true);
  assert(l > 0);
  len = (size_t)l;
  if ((ret = dds_write(com->wr_request, request)) < 0) {
    DDS_ERROR("failed to publish dc_request %s%s [%s]", str, (len >= sizeof(str)) ? "..(trunc)" : "", dds_strretcode(-ret));
    goto err_request_write;
  }
  DDS_CLOG(DDS_LC_DUR, &dc.gv->logconfig, "publish dc_request %s%s\n", str, (len >= sizeof(str)) ? "..(trunc)" : "");
err_request_write:
  DurableSupport_request_free(request, DDS_FREE_ALL);
  return ret;
}

static dds_return_t dc_com_request_dispose (struct com_t *com, dds_instance_handle_t ih)
{
  return dds_dispose_ih(com->wr_request, ih);
}

static void dc_server_lost (struct dc_t *dc, DurableSupport_status *status)
{
  struct server_t *server;
  uint32_t total;

  /* lookup the ds entry in the list of available ds's */
  if ((server = ddsrt_avl_clookup (&server_td, &dc->servers, status->id)) == NULL)  {
    /* ds not known, so nothing lost */
    return;
  }
  ddsrt_avl_cdelete(&server_td, &dc->servers, server);
  total = (uint32_t)ddsrt_avl_ccount(&dc->servers);
  DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "durable service \"%s\" (%s@%s) lost (total: %" PRIu32 ")\n", server->id_str, server->name, server->hostname, total);
  cleanup_server(server);
}

static void dc_server_discovered (struct dc_t *dc, DurableSupport_status *status)
{
  struct server_t *server;
  uint32_t total;

  /* lookup the ds entry in the list of available ds's */
  if ((server = ddsrt_avl_clookup (&server_td, &dc->servers, status->id)) != NULL)  {
    /* ds already known */
    return;
  }
  server = create_server(dc, status->id, status->name, status->hostname);
  total = (uint32_t)ddsrt_avl_ccount(&dc->servers);
  DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "durable service \"%s\" (%s@%s) discovered (total: %" PRIu32 ")\n", server->id_str, server->name, server->hostname, total);
}

static int dc_process_status (dds_entity_t rd, struct dc_t *dc)
{
#define MAX_SAMPLES   100

  void *samples[MAX_SAMPLES];
  dds_sample_info_t info[MAX_SAMPLES];
  int samplecount;
  int j;

  /* dds_read/take allocates memory for the data if samples[0] is a null pointer.
   * The memory must be released when done by returning the loan */
  samples[0]= NULL;
  samplecount = dds_take_mask (rd, samples, info, MAX_SAMPLES, MAX_SAMPLES, DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
  if (samplecount < 0) {
    DDS_ERROR("durable client failed to take ds_status [%s]", dds_strretcode(-samplecount));
  } else {
    /* call the handler function to process the status sample */
    for (j = 0; !dds_triggered(dc->com->ws) && j < samplecount; j++) {
      DurableSupport_status *status = (DurableSupport_status *)samples[j];
      if ((info[j].instance_state == DDS_IST_NOT_ALIVE_DISPOSED) || (info[j].instance_state == DDS_IST_NOT_ALIVE_NO_WRITERS)) {
        /* a DS is not available any more, remove from the list of known DS's */
        dc_server_lost(dc, status);
      } else if (info[j].valid_data) {
        /* To avoid reacting to malicious status messages we want to verify if
         * the status message originates from a "real" DS by verifying if its participant
         * contains the IDENT in the user data. To do this we actually need to retrieve
         * the builtin endpoint that represents the status writer that published this
         * status, and check if the participant of this writer has the IDENT in its
         * userdata. For now, we skip this check. */
        dc_server_discovered(dc, status);
      }
    }
    (void)dds_return_loan(rd, samples, samplecount);
  }
  return samplecount;
#undef MAX_SAMPLES
}

/* lookup the delivery context for the ds identified by id
 * if not found and autocreate=true, then create the delivery context */
static struct delivery_ctx_t *dc_get_delivery_ctx (struct dc_t *dc, DurableSupport_id_t id, bool autocreate)
{
  struct delivery_ctx_t *delivery_ctx = NULL;

  if ((dc->delivery_ctx != NULL) && (memcmp(dc->delivery_ctx->id,id,16) == 0)) {
    delivery_ctx = dc->delivery_ctx;
  } else  if (((delivery_ctx = ddsrt_avl_clookup (&delivery_ctx_td, &dc->delivery_ctxs, id)) == NULL) && autocreate) {
    delivery_ctx = ddsrt_malloc(sizeof(struct delivery_ctx_t));
    memset(delivery_ctx, 0, sizeof(struct delivery_ctx_t));
    memcpy(delivery_ctx->id,id,16);
    ddsrt_avl_cinit(&delivery_readers_td, &delivery_ctx->readers);
    ddsrt_avl_cinsert(&delivery_ctx_td, &dc->delivery_ctxs, delivery_ctx);
  }
  return delivery_ctx;
}

/* Remove the delivery identified by id */
static void dc_remove_delivery_ctx (struct dc_t *dc, DurableSupport_id_t id)
{
  struct delivery_ctx_t *delivery_ctx;
  ddsrt_avl_dpath_t dpath;

  if ((delivery_ctx = ddsrt_avl_clookup_dpath(&delivery_ctx_td, &dc->delivery_ctxs, id, &dpath)) != NULL) {
    ddsrt_avl_cdelete_dpath(&delivery_ctx_td, &dc->delivery_ctxs, delivery_ctx, &dpath);
    if (delivery_ctx == dc->delivery_ctx) {
      dc->delivery_ctx = NULL;
    }
    cleanup_delivery_ctx(delivery_ctx);
  }
}

static struct delivery_reader_t *create_delivery_reader (dds_guid_t *guid)
{
  struct delivery_reader_t *delivery_reader;

  delivery_reader = ddsrt_malloc(sizeof(struct  delivery_reader_t));
  memcpy(delivery_reader->key.rguid.v, guid->v, 16);
  return delivery_reader;
}

static struct delivery_reader_t *dc_get_reader_from_delivery_ctx (struct delivery_ctx_t *delivery_ctx, DurableSupport_id_t id, bool autocreate)
{
  struct delivery_reader_t *delivery_reader;
  ddsrt_avl_ipath_t path;
  dds_guid_t key;

  memcpy(key.v, id, 16);
  /* create a container for the topic */
  if (((delivery_reader = ddsrt_avl_clookup_ipath(&delivery_readers_td, &delivery_ctx->readers, &key, &path)) == NULL) && (autocreate)) {
    delivery_reader = create_delivery_reader(&key);
    ddsrt_avl_cinsert_ipath(&delivery_readers_td, &delivery_ctx->readers, delivery_reader, &path);
  }
  return delivery_reader;
}

static void unblock_wfhd_for_reader (struct dc_t *dc, dds_entity_t reader)
{
  dds_entity *e;
  dds_reader *rd;
  dds_return_t rc;
  dds_guid_t rguid;
  char id_str[37];

  if ((rc = dds_entity_lock (reader, DDS_KIND_READER, &e)) != DDS_RETCODE_OK) {
    /* The reader could not be found, it may have been deleted.
     * Since this is legitimate, we silently return from this function. */
    return;
  }
  if ((rc = dds_get_guid(reader, &rguid)) != DDS_RETCODE_OK) {
    DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "failed to retrieve reader guid for reader with handle %" PRId32 "\n", reader);
    goto err_get_guid;
  }
  DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "unblock wfhd for reader \"%s\"\n", dc_stringify_id(rguid.v, id_str));
  rd = (dds_reader *)e;
  /* unblock wfhd for this reader */
  ddsrt_mutex_lock(&rd->wfhd_mutex);
  ddsrt_cond_broadcast(&rd->wfhd_cond);
  ddsrt_mutex_unlock(&rd->wfhd_mutex);
  dds_entity_unlock(e);
  return;

err_get_guid:
  dds_entity_unlock(e);
  return;
}

/* Close the current delivery context for the ds identified by id */
static void dc_close_delivery (struct dc_t *dc, DurableSupport_id_t id, DurableSupport_response_set_t *response_set)
{
  char id_str[37];
  char str[1024];

  assert(response_set);
  assert(response_set->flags & DC_FLAG_SET_END);
  (void)response_set;  /* to silence the compiler for release builds */
  /* Lookup the delivery context for this aligner */
  if ((dc->delivery_ctx = dc_get_delivery_ctx(dc, id, false)) == NULL) {
    /* There does not exists a delivery context for the DS that produced the response. */
    DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "unable to close delivery for ds \"%s\"\n", dc_stringify_id(id, id_str));
    return;
  }
  /* There exists a delivery context for the ds identified by id, close this delivery context */
  (void)dc_stringify_delivery_ctx(str, sizeof(str), dc->delivery_ctx);
  DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "close delivery %s\n", str);
  /* remove current delivery ctx */
  dc_remove_delivery_ctx(dc, id);
}

/* Abort the current delivery context for the ds identified by id
 *
 * The current delivery is aborted when a new response set is received without the
 * previous set being ended correctly. */
static void dc_abort_delivery (struct dc_t *dc, DurableSupport_id_t id, DurableSupport_response_set_t *response_set)
{
  char id_str[37];
  char str[1024];

  DC_UNUSED_ARG(response_set);
  /* lookup the align context for this aligner */
  if ((dc->delivery_ctx = dc_get_delivery_ctx(dc, id, false)) == NULL) {
    /* There exists a delivery context for the ds that produced the response. */
    DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "unable to abort delivery for unknown delivery from ds \"%s\"\n", dc_stringify_id(id, id_str));
    return;
  }
  assert(memcmp(dc->delivery_ctx->id,id,16) == 0);
  (void)dc_stringify_delivery_ctx(str, sizeof(str), dc->delivery_ctx);
  DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "abort delivery %s", str);
  /* remove current delivery ctx */
  dc_remove_delivery_ctx(dc, id);
}

/* open the delivery context for the ds identified by id */
static void dc_open_delivery (struct dc_t *dc,  DurableSupport_id_t id, DurableSupport_response_set_t *response_set)
{
  char id_str[37];
  char str[1024];
  uint32_t i;

  assert(response_set);
  assert(response_set->flags & DC_FLAG_SET_BEGIN);
  /* lookup the align context for this aligner */
  if ((dc->delivery_ctx = dc_get_delivery_ctx(dc, id, false)) != NULL) {
    /* There already exists a delivery context for the ds that produced the response.
     * Implicitly abort it before opening a new one.*/
    dc_abort_delivery(dc, id, response_set);
  }
  /* Now create the new delivery context */
  if ((dc->delivery_ctx = dc_get_delivery_ctx(dc, id, true)) == NULL) {
    DDS_ERROR("unable to create an delivery context for ds \"%s\"\n", dc_stringify_id(id, id_str));
    abort();
  }
  assert(dc->delivery_ctx);
  /* Collect the readers for which the response is intended in the delivery context.
   * A response set usually carries a non-empty set of reader guids without duplicates.
   * However, whenever a response set accidentally contains duplicate reader guids or
   * no readers at all, we don't want this to cause any trouble, so we will handle
   * these cases as well.
   * In case a response contains duplicate reader guids, then we will administrate
   * the reader only once.
   * In case a response is published with an empty reader list, the readers in the
   * delivery context will be empty. Any response data belonging to such set will simply
   * not be delivered to any reader. */
  for (i=0; i < response_set->guids._length; i++) {
    (void)dc_get_reader_from_delivery_ctx(dc->delivery_ctx, response_set->guids._buffer[i], true);
  }
  (void)dc_stringify_delivery_ctx(str, sizeof(str), dc->delivery_ctx);
  DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "open delivery %s\n", str);
}

static void dc_process_set_response_begin(struct dc_t *dc, DurableSupport_response *response)
{
  assert(response);
  assert(response->body._d == DurableSupport_RESPONSETYPE_SET);
  /* A response set begin is received. If there already exists an open delivery
   * context for this ds which has not been ended correctly, then this
   * previous delivery has failed and needs to be aborted.  */
  if ((dc->delivery_ctx = dc_get_delivery_ctx(dc, response->id, false)) != NULL) {
    assert(memcmp(dc->delivery_ctx->id, response->id, 16) == 0);
    DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "current open delivery %" PRIu64 " has missed an end\n", dc->delivery_ctx->delivery_id);
    /* abort the currently open delivery */
    dc_abort_delivery(dc, dc->delivery_ctx->id, &response->body._u.set);
  }
  assert(dc->delivery_ctx == NULL);
  /* open the new delivery */
  dc_open_delivery(dc, response->id, &response->body._u.set);
}

static void dc_process_set_response_end (struct dc_t *dc, DurableSupport_response *response)
{
  char id_str[37];

  assert(response);
  assert(response->body._d == DurableSupport_RESPONSETYPE_SET);
  if ((dc->delivery_ctx = dc_get_delivery_ctx(dc, response->id, false)) != NULL) {
    assert(memcmp(dc->delivery_ctx->id, response->id, 16) == 0);
    /* There exists an open delivery from the DS.
     * Now check if end delivery id corresponds to the open delivery id. */
    if (dc->delivery_ctx->delivery_id != response->body._u.set.delivery_id) {
      DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "current open delivery %" PRIu64 " does not match with end delivery %" PRIu64 "\n", dc->delivery_ctx->delivery_id, response->body._u.set.delivery_id);
      /* abort the currently open delivery */
      dc_abort_delivery(dc, dc->delivery_ctx->id, &response->body._u.set);
    }
    dc_close_delivery(dc, response->id, &response->body._u.set);
  } else {
    DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "unable to close delivery %" PRIu64 " from ds \"%s\"\n", response->body._u.set.delivery_id, dc_stringify_id(response->id, id_str));
  }
}

static void dc_process_reader_response (struct dc_t *dc, DurableSupport_response *response)
{
  struct delivery_request_key_t key;
  struct delivery_request_t *dr;
  ddsrt_avl_dpath_t dpath;

  assert(response);
  assert(response->body._d == DurableSupport_RESPONSETYPE_READER);
  memcpy(key.guid.v, response->body._u.reader.rguid, 16);
  /* Lookup if there is a delivery request for this reader.
   * If so, unblock the reader. */
  if ((dr = ddsrt_avl_clookup_dpath(&delivery_requests_td, &dc->delivery_requests, &key, &dpath)) != NULL) {
    unblock_wfhd_for_reader(dc, dr->reader);
  }
}

static void dc_process_set_response (struct dc_t *dc, DurableSupport_response *response)
{
  assert(response->body._d == DurableSupport_RESPONSETYPE_SET);
  if (response->body._u.set.flags &  DC_FLAG_SET_BEGIN) {
    dc_process_set_response_begin(dc, response);
  } else if (response->body._u.set.flags &  DC_FLAG_SET_END) {
    dc_process_set_response_end(dc, response);
  } else {
    DDS_ERROR("invalid response set flag 0x%04" PRIx32, response->body._u.set.flags);
  }
}

/* todo: this function is hared between ds and client */
static enum ddsi_serdata_kind get_serdata_kind (uint8_t kind)
{
  enum ddsi_serdata_kind result = SDK_EMPTY;

  if (kind == 0) {
    result = SDK_EMPTY;
  } else if (kind == 1) {
    result = SDK_KEY;
  } else if (kind == 2) {
    result = SDK_DATA;
  } else {
    DDS_ERROR("Invalid serdata kind %d", kind);
    abort();
  }
  return result;
}

/* These are the offsets used in the data responses.
 * TODO: These definitions should actually be shared between the ds and the client.
 */
#define RESPONSE_HEADER_OFFSET_WT                    8
#define RESPONSE_HEADER_OFFSET_SEQNUM               16
#define RESPONSE_HEADER_OFFSET_WRITER_GUID          24
#define RESPONSE_HEADER_OFFSET_SERDATA_KIND         40
#define RESPONSE_HEADER_OFFSET_SERDATA_AUTODISPOSE  41
#define RESPONSE_HEADER_OFFSET_SERDATA              42


static void dc_process_data_response (struct dc_t *dc, DurableSupport_response *response)
{
  ddsrt_avl_citer_t it;
  const struct ddsi_sertype *sertype;
  struct ddsi_serdata *serdata;
  uint32_t response_size, serdata_size;;
  uint32_t serdata_offset;
  ddsrt_iovec_t data_out;
  int64_t wt;
  uint64_t seqnum;
  enum ddsi_serdata_kind serdata_kind;
  dds_guid_t wguid;
  dds_return_t ret = DDS_RETCODE_OK;
  bool autodispose = 0;
  char id_str[37];
  struct delivery_reader_t *delivery_reader;

  /* TODO:
   * A DS also has a response reader (by virtue of the CycloneDDS instance it runs).
   * A DS therefore also receives responses that it sends to itself.
   * Preferably we should prevent that the responses are end up at DSs.
   * In any case, if responses do end up a DS, then the DS should ignore it.  */
  if ((dc->delivery_ctx = dc_get_delivery_ctx(dc, response->id, false)) == NULL) {
    /* There is no delivery context for this data, which means that this node
     * did not request data for this set. If we do receive data, then we
     * can safely ignore it. */
    return;
  }
  if (ddsrt_avl_cis_empty(&dc->delivery_ctx->readers)) {
    /* There are no readers to deliver the data to. */
    return;
  }
  /* A response_set begin has been received. Because data delivery is reliable,
   * we are sure that we missed no responses, so the data response that we received
   * must belong to the proxy set. */
  response_size = response->body._u.data.blob._length;
  serdata_offset = ddsrt_fromBE4u(*((uint32_t *)response->body._u.data.blob._buffer));
  wt = ddsrt_fromBE8(*((int64_t *)(response->body._u.data.blob._buffer + RESPONSE_HEADER_OFFSET_WT)));
  seqnum = ddsrt_fromBE8u(*((uint64_t *)(response->body._u.data.blob._buffer + RESPONSE_HEADER_OFFSET_SEQNUM)));
  memcpy(&wguid.v, response->body._u.data.blob._buffer + RESPONSE_HEADER_OFFSET_WRITER_GUID, 16);
  autodispose = *((uint8_t *)response->body._u.data.blob._buffer + RESPONSE_HEADER_OFFSET_SERDATA_AUTODISPOSE);
  serdata_kind = get_serdata_kind(*((uint8_t *)response->body._u.data.blob._buffer + RESPONSE_HEADER_OFFSET_SERDATA_KIND));
  /* We could now potentially figure out if the response that has been received
   * contains fields that we cannot interpret. We can find that out by comparing
   * the received response_offset with my own RESPONSE_HEADER_OFFSET_SERDATA.
   * If serdata_offset > RESPONSE_HEADER_OFFSET_SERDATA then this is an indication
   * that the ds has more fields than this client can interpret. */
  /* get the serdata */
  serdata_size = response_size - serdata_offset;
  data_out.iov_len = serdata_size;
  data_out.iov_base = response->body._u.data.blob._buffer + serdata_offset;
  /* Now deliver the data to the local readers. */
  for (delivery_reader = ddsrt_avl_citer_first (&delivery_readers_td, &dc->delivery_ctx->readers, &it); delivery_reader; delivery_reader = ddsrt_avl_citer_next (&it)) {
    /* Lookup the reader entity that requested the delivery. */
    /* check if the reader still exists by lookup the entity */
    struct delivery_request_t *dr;
    struct delivery_request_key_t key;
    dds_entity_t reader;

    memcpy(key.guid.v, delivery_reader->key.rguid.v, 16);
    if ((dr = ddsrt_avl_clookup(&delivery_requests_td, &dc->delivery_requests, &key)) == NULL) {
      /* Unable to find a delivery request for this reader, so
       * we cannot resolve the reader entity associated with the guid.
       * Evidently, we cannot inject data into the rhc of a reader
       * that we cannot resolve. */
      continue;
    }
    reader = dr->reader;
    /* in order to insert the data in the rhc of the reader we first
     * need to resolve the type of the reader */
    if ((ret = dds_get_entity_sertype (reader, &sertype)) < 0) {
      /* We failed to get the sertype of the reader.
       * This could be because the reader does not exist any more,
       * but frankly I don't care about  the reason. All that matters is
       * that we cannot inject data into the rhc of this reader. */
      continue;
    }
    /* get the serdata */
    if ((serdata = ddsi_serdata_from_ser_iov (sertype, serdata_kind, 1, &data_out, serdata_size)) == NULL) {
      /* Failed to get the serdata. If it happens I do not consider
       * this my problem, it is a problem in CycloneDDS. The only thing
       * I can do is to handle this case. For now I silently ignore the data.
       */
      goto err_serdata;
    }
    serdata->sequence_number = seqnum;
    serdata->timestamp.v = wt;
    memcpy(&serdata->writer_guid, &wguid, 16);
    if ((ret = dds_reader_store_historical_serdata(reader, wguid, autodispose, serdata)) != DDS_RETCODE_OK) {
      DDS_ERROR("Failed to deliver historical data to reader \"%s\" [%s]\n", dc_stringify_id(dr->key.guid.v, id_str), dds_strretcode(ret));
      goto err_store_historical_serdata;
    }
    ddsi_serdata_to_ser_unref(serdata, &data_out);
  }
  return;

err_store_historical_serdata:
  ddsi_serdata_to_ser_unref(serdata, &data_out);
err_serdata:
 return;
}

static int dc_process_response (dds_entity_t rd, struct dc_t *dc)
{
#define MAX_SAMPLES   100

  void *samples[MAX_SAMPLES];
  dds_sample_info_t info[MAX_SAMPLES];
  int samplecount;
  int j;

  /* dds_read/take allocates memory for the data if samples[0] is a null pointer.
   * The memory must be released when done by returning the loan */
  samples[0] = NULL;
  samplecount = dds_take_mask (rd, samples, info, MAX_SAMPLES, MAX_SAMPLES, DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);

  if (samplecount < 0) {
    DDS_ERROR("failed to take dc_response [%s]", dds_strretcode(-samplecount));
  } else {
    /* process the response
     * we ignore invalid samples and only process valid responses */
    for (j = 0; !dds_triggered(dc->com->ws) && j < samplecount; j++) {
      DurableSupport_response *response = (DurableSupport_response *)samples[j];
      char str[1024] = { 0 };  /* max string representation size */
      int l;
      if (info[j].valid_data && ((l = dc_stringify_response(str, sizeof(str), response)) > 0)) {
        size_t len = (size_t)l;

        /* LH: TODO: add statistics for responses */
        DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "dc_response %s%s\n", str, (len >= sizeof(str)) ? "..(trunc)" : "");
        /* deliver the received response to the readers belonging to the proxy set */
        switch (response->body._d) {
          case DurableSupport_RESPONSETYPE_SET:
            dc_process_set_response(dc, response);
            break;
          case DurableSupport_RESPONSETYPE_READER:
            dc_process_reader_response(dc, response);
            break;
          case DurableSupport_RESPONSETYPE_DATA:
            dc_process_data_response(dc, response);
            break;
          default :
            DDS_ERROR("invalid response type %" PRIu16, response->body._d);
        }
      }
    }
    (void)dds_return_loan(rd, samples, samplecount);
  }
  return samplecount;

#undef MAX_SAMPLES
}

static void dc_delete_delivery_request (struct dc_t *dc, dds_guid_t rguid)
{
  struct delivery_request_t *dr;
  struct delivery_request_key_t key;
  ddsrt_avl_dpath_t dpath;
  char dr_str[512];

  memcpy(key.guid.v, rguid.v, 16);
  if ((dr = ddsrt_avl_clookup_dpath (&delivery_requests_td, &dc->delivery_requests, &key, &dpath)) != NULL) {
    /* remove from fibheap */
    ddsrt_fibheap_delete(&delivery_requests_fd, &dc->delivery_requests_fh, dr);
    /* remove from delivery requests */
    ddsrt_avl_cdelete_dpath(&delivery_requests_td, &dc->delivery_requests, dr, &dpath);
    (void)dc_stringify_delivery_request(dr_str, sizeof(dr_str), dr);
    DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "delete delivery request %s\n", dr_str);
    cleanup_delivery_request(dr);
  }
}

static struct delivery_request_t *dc_insert_delivery_request (struct dc_t *dc, dds_entity_t reader, dds_guid_t rguid)
{
  struct delivery_request_t *dr;
  struct delivery_request_key_t key;
  ddsrt_avl_ipath_t ipath;
  char dr_str[512];
  dds_return_t rc;

  memcpy(key.guid.v, rguid.v, 16);
  if ((dr = ddsrt_avl_clookup_ipath (&delivery_requests_td, &dc->delivery_requests, &key, &ipath)) == NULL) {
    dr = ddsrt_malloc(sizeof(struct delivery_request_t));
    memcpy(dr->key.guid.v, key.guid.v, 16);
    dr->reader = reader;
    dr->exp_time = DDS_NEVER;
    (void)dc_stringify_delivery_request(dr_str, sizeof(dr_str), dr);
    DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "create delivery request %s\n", dr_str);
    ddsrt_avl_cinsert_ipath(&delivery_requests_td, &dc->delivery_requests, dr, &ipath);
    ddsrt_fibheap_insert(&delivery_requests_fd, &dc->delivery_requests_fh, dr);
    if (dr == ddsrt_fibheap_min(&delivery_requests_fd, &dc->delivery_requests_fh)) {
      /* delivery request added to the front, trigger guard condition */
      if ((rc = dds_set_guardcondition(dc->com->delivery_request_guard, true)) < 0) {
        DDS_ERROR("failed to set delivery request guard to true [%s]", dds_strretcode(rc));
      }
    }
  }
  return dr;
}

static void dc_send_request_for_reader (struct dc_t *dc, dds_entity_t reader, struct dds_rhc *rhc)
{
  dds_return_t rc;
  dds_guid_t rguid;

  (void)rhc;
  if ((rc = dds_get_guid(reader, &rguid)) < DDS_RETCODE_OK) {
    DDS_ERROR("Unable to retrieve the guid of the reader [%s]", dds_strretcode(-rc));
    goto err_get_guid;
  }
  /* Remember the delivery request for this reader.
   * This is used a.o. to correlate reader guids to actual readers. */
  dc_insert_delivery_request(dc, reader, rguid);
  /* Publish the quest */
  if ((rc = dc_com_request_write(dc->com, rguid)) != DDS_RETCODE_OK) {
    DDS_ERROR("Failed to publish dc_request [%s]", dds_strretcode(-rc));
    /* We failed to request data for this proxy set.
     * We could do several things now, e.g., 1) try again until we succeed,
     * 2) notify the application that no historical data could be retrieved,
     * or 3) commit suicide because we cannot guarantee eventual consistency.
     * For now, I choose for the latter. After all, it is expected that sending
     * a request should never fail. */
    dc_delete_delivery_request(dc, rguid);
    abort();
  }
  return;

err_get_guid:
  return;
}

/* called when there is a match for a durable writer */
static void default_durable_writer_matched_cb (dds_entity_t writer, dds_publication_matched_status_t status, void *arg)
{
  struct dc_t *dc = (struct dc_t *)arg;
  dds_instance_handle_t ih;
  dds_builtintopic_endpoint_t *ep;

  /* a reader has matched with a durable writer. */
  /* check if the reader is a data container.
   * If so, this might affect the quorum */
  assert(writer);
  if ((ih = status.last_subscription_handle) == DDS_HANDLE_NIL) {
    DDS_ERROR("failed to receive valid last_subscription_handle\n");
    goto err_last_subscription_handle;
  }
  if ((ep = dds_get_matched_subscription_data(writer, ih)) != NULL) {
    if (dc_is_ds_endpoint(dc->com, ep, dc->cfg.ident)) {
      /* A data container has matched with this writer.
       * Check if the quorum is met. */
      dc_check_quorum_reached(dc, writer, true);
    }
    dds_builtintopic_free_endpoint(ep);
  } else {
    /* the endpoint is not available any more.
     * check if the quorum has been lost */
    dc_check_quorum_reached(dc, writer, false);
  }
err_last_subscription_handle:
  return;
}

/* dispose the delivery request */
static void dc_dispose_delivery_request (struct dc_t *dc, struct delivery_request_t *dr)
{
  dds_return_t rc;
  dds_instance_handle_t ih;
  DurableSupport_request request;
  char id_str[37];

  DC_UNUSED_ARG(dr);
  memcpy(request.key.rguid, dr->key.guid.v, 16);
  if ((ih = dds_lookup_instance(dc->com->wr_request, &request)) == DDS_HANDLE_NIL) {
    DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "no delivery request for reader \"%s\" found, unable to dispose\n", dc_stringify_id(request.key.rguid, id_str));
    goto err_dispose_delivery_request;
  }
  if ((rc = dc_com_request_dispose(dc->com, ih)) != DDS_RETCODE_OK) {
    DDS_ERROR("failed to dispose delivery request for reader \"%s\" [%s]\n", dc_stringify_id(request.key.rguid, id_str), dds_strretcode(-rc));
    goto err_dispose_delivery_request;
  }
  DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "dispose delivery request for reader \"%s\"\n", dc_stringify_id(request.key.rguid, id_str));
err_dispose_delivery_request:
  return;
}

/* a dc_request reader endpoint on a DS has been found that matched with my dc_request writer */
static struct matched_request_reader_t *dc_add_matched_request_reader (struct dc_t *dc, dds_builtintopic_endpoint_t *ep, dds_instance_handle_t ih)
{
  struct matched_request_reader_t *mrr;
  uint32_t cnt;
  char id_str[37];

  assert(ep);
  if ((mrr = ddsrt_avl_clookup (&matched_request_readers_td, &dc->matched_request_readers, &ih)) == NULL) {
    mrr = (struct matched_request_reader_t *)ddsrt_malloc(sizeof(struct matched_request_reader_t));
    mrr->ih = ih;
    ddsrt_avl_cinsert(&matched_request_readers_td, &dc->matched_request_readers, mrr);
    cnt = (uint32_t)ddsrt_avl_ccount(&dc->matched_request_readers);
    dc->nr_of_matched_dc_requests = cnt;
    DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "matching dc_request reader \"%s\" discovered (ih: %" PRIx64 ") [%" PRIu32 "]\n", dc_stringify_id(ep->key.v, id_str), ih, cnt);
  }
  return mrr;
}

/* a dc_request reader has been lost */
static void dc_remove_matched_request_reader (struct dc_t *dc, dds_instance_handle_t ih)
{
  struct matched_request_reader_t *mrr;
  ddsrt_avl_dpath_t dpath;
  uint32_t cnt;

  if ((mrr = ddsrt_avl_clookup_dpath(&matched_request_readers_td, &dc->matched_request_readers, &ih, &dpath)) != NULL) {
    ddsrt_avl_cdelete_dpath(&matched_request_readers_td, &dc->matched_request_readers, mrr, &dpath);
    cleanup_matched_request_reader(mrr);
    cnt = (uint32_t)ddsrt_avl_ccount(&dc->matched_request_readers);
    dc->nr_of_matched_dc_requests = cnt;
    DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "matching dc_request reader lost (ih: %" PRIx64 ") [%" PRIu32 "]\n", ih, cnt);
  }
}

/* called when the dc_request writer has been found or lost a matching dc_request reader.
 * Due to synchronous triggering we see them all  */
static void default_request_writer_matched_cb (dds_entity_t writer, dds_publication_matched_status_t status, void *arg)
{
  struct dc_t *dc = (struct dc_t *)arg;
  dds_instance_handle_t ih;
  dds_builtintopic_endpoint_t *ep;

  /* A reader has matched (or lost) with a dc_request writer. */
  /* As long as there is at least one match with a dc_request
   * reader located on a ds, we can publish safely publish
   * requests. If there is no match with such dc_request reader,
   * we have to postpone the publication of dc_request until such
   * dc_request reader becomes available. */
  assert(writer);
  if ((ih = status.last_subscription_handle) == DDS_HANDLE_NIL) {
    DDS_ERROR("failed to receive valid last_subscription_handle\n");
    return;
  }
  if ((ep = dds_get_matched_subscription_data(writer, ih)) != NULL) {
    if (dc_is_ds_endpoint(dc->com, ep, dc->cfg.ident)) {
      /* A dc_request reader on a DS has matched with the dc_request writer.
       * From now on it is allowed to publish requests.
       * In case there are any delivery requests, we can sent them now */
      dc_add_matched_request_reader(dc, ep, ih);
    }
    dds_builtintopic_free_endpoint(ep);
  } else {
    /* the dc_request endpoint is not available any more. */
    dc_remove_matched_request_reader(dc, ih);
  }
}

/* handle expired delivery request and determine the next timeout */
static void dc_process_delivery_request_guard (struct dc_t *dc, dds_time_t *timeout)
{
  dds_return_t ret;
  struct delivery_request_t *dr;
  dds_time_t now = dds_time();
  char dr_str[256];

  ddsrt_mutex_lock(&dc->delivery_request_mutex);
  do {
    dr = ddsrt_fibheap_min(&delivery_requests_fd, &dc->delivery_requests_fh);
    if ((dr == NULL) || (now < dr->exp_time)) {
      /* there is no delivery request, or the first delivery request has not yet expired */
      break;
    }
    /* The head is expired. Remove the delivery request from the priority queue
     * and dispose it to prevent that a DS reacts to an expired request */
    if ((dr = ddsrt_fibheap_extract_min(&delivery_requests_fd, &dc->delivery_requests_fh)) != NULL) {
      (void)dc_stringify_delivery_request(dr_str, sizeof(dr_str), dr);
      DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "process delivery request \"%s\"\n", dr_str);
      dc_dispose_delivery_request(dc, dr);
      dc_delete_delivery_request(dc, dr->key.guid);
    }
  } while (true);
  /* recalculate the remaining timeout */
  *timeout = (dr == NULL) ? DDS_NEVER : dr->exp_time;
  if ((ret = dds_set_guardcondition(dc->com->delivery_request_guard, false)) < 0) {
    DDS_ERROR("failed to set delivery request guard to false [%s]", dds_strretcode(ret));
  }
  if (*timeout != DDS_NEVER) {
    dds_duration_t tnext = *timeout - now;
    int64_t sec = (int64_t)(tnext / DDS_NSECS_IN_SEC);
    uint32_t usec = (uint32_t)((tnext % DDS_NSECS_IN_SEC) / DDS_NSECS_IN_USEC);
    DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "next delivery request timeout at %" PRId64 ".%06" PRIu32 "s\n", sec, usec);
  }
  ddsrt_mutex_unlock(&dc->delivery_request_mutex);
}

static uint32_t delivery_handler (void *a)
{
  struct dc_t *dc = (struct dc_t *)a;
  dds_time_t timeout = DDS_NEVER;
  dds_attach_t wsresults[3];
  size_t wsresultsize = sizeof(wsresults) / sizeof(wsresults[0]);
  int n, j;

  DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "start durable client thread\n");
  while (!dds_triggered(dc->com->ws)) {
    n = dds_waitset_wait_until (dc->com->ws, wsresults, wsresultsize, timeout);
    if (n < 0) {
      DDS_ERROR("Error in dds_waitset_wait_until [%s]\n", dds_strretcode(n));
    } else if (n > 0) {
      for (j=0; j < n && (size_t)j < wsresultsize; j++) {
        if (wsresults[j] == dc->com->rd_status) {
          dc_process_status(dc->com->rd_status, dc);
        } else if (wsresults[j] == dc->com->rd_response) {
          dc_process_response(dc->com->rd_response, dc);
        } else if (wsresults[j] == dc->com->delivery_request_guard) {
          dc_process_delivery_request_guard(dc, &timeout);
        }
      }
    } else {
      /* timeout */
      dc_process_delivery_request_guard(dc, &timeout);
    }
  }
  DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "stop durable client thread\n");
  return 0;
}

/* set the request listener to learn about the existence of matching request readers.
 * This purpose of this function is acquire matched subscriptions for late joining
 * request writers. */
static dds_return_t activate_request_listener (struct dc_t *dc)
{
  dds_return_t rc, ret;
  dds_guid_t wguid;
  char id_str[37];
  size_t nrds = 128;
  dds_instance_handle_t *rd_ihs;
  int i;
  dds_builtintopic_endpoint_t *ep;
  bool selected = false;  /* indicates if a dc_request reader is selected to accept our requests */

  assert(dc);
  assert(dc->com);
  if ((rc = dds_get_guid(dc->com->wr_request, &wguid)) != DDS_RETCODE_OK) {
    DDS_ERROR("failed to retrieve writer guid for request writer");
    goto err_get_guid;
  }
  if ((rc = dds_set_listener(dc->com->wr_request, dc->request_listener)) < 0) {
    DDS_ERROR("Unable to set the request listener on writer \"%s\"\n", dc_stringify_id(wguid.v, id_str));
    goto err_set_listener;
  }
  /* matches for this listener may have occurred before the listener was attached.
   * To get these matches, we need to call dds_get_matched_subscriptions().
   * We don't know the size of the array holding the matches beforehand, so
   * we dynamically this list. */
  do {
    nrds = nrds * 2;
    rd_ihs = ddsrt_malloc(nrds * sizeof(dds_instance_handle_t));
    if ((ret = dds_get_matched_subscriptions(dc->com->wr_request, rd_ihs, nrds)) > (int32_t)nrds) {
      /* allocated list is too small, use a bigger list */
      ddsrt_free(rd_ihs);
    }
  } while (ret > (int32_t)nrds);
  /* The local host request reader only match requests with a ds on the same host.
   * As long as there is at least one match, then we know there is a dc_request reader
   * on the local node. If the dc_request reader belongs to a ds, then we have found
   * a ds on the local node. */
  DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "request listener activated, currently found %" PRIu32 " matching dc_request readers\n", (uint32_t)ret);
  for (i=0; i < ret && !selected; i++) {
    /* check if the matched reader belongs to a ds  */
    if ((ep = dds_get_matched_subscription_data(dc->com->wr_request, rd_ihs[i])) != NULL) {
      if (dc_is_ds_endpoint(dc->com, ep, dc->cfg.ident)) {
        /* A dc_request reader on a DS has matched with the dc_request writer.
         * From now on it is allowed to publish requests.
         * In case there are any delivery requests, we can sent them now */
        dc_add_matched_request_reader(dc, ep, rd_ihs[i]);
      }
      dds_builtintopic_free_endpoint(ep);
    } else {
      /* the dc_request endpoint is not available any more. */
      dc_remove_matched_request_reader(dc, rd_ihs[i]);
    }
  }
  ddsrt_free(rd_ihs);
  return rc;

err_set_listener:
err_get_guid:
  return DDS_RETCODE_ERROR;
}

static dds_return_t dds_durability_init (const dds_domainid_t domainid, struct ddsi_domaingv *gv)
{
  dds_return_t rc;

  /* a participant is created, increase the refcount.
   * If this is not the first participant, then there is
   * no reason to initialize the durable client */
  if (ddsrt_atomic_inc32_nv(&dc.refcount) > 1) {
    return DDS_RETCODE_OK;
  }
  /* This is the first participant, so let's create a durable client (dc).
   * The dc will also create a new participant, therefore
   * increase the refcount for this participant as well.
   * The guid of the participant for client durability will be used
   * to identify this client. */
  ddsrt_atomic_inc32(&dc.refcount);
  dc.gv = gv;
  /* Note: dc.cfg.id will be set once we create the participant in dc_com_new() */
  dc.cfg.quorum = DEFAULT_QOURUM;  /* LH: currently hardcoded set to 1, should be made configurable in future */
  dc.cfg.ident = ddsrt_strdup(DEFAULT_IDENT);
  dc.selected_request_reader_ih = DDS_HANDLE_NIL;
  dc.nr_of_matched_dc_requests = 0;
  dc.delivery_ctx = NULL; /* initially no current open delivery context */
  ddsrt_avl_cinit(&delivery_ctx_td, &dc.delivery_ctxs);
  ddsrt_avl_cinit(&server_td, &dc.servers);
  ddsrt_avl_cinit(&matched_request_readers_td, &dc.matched_request_readers);
  ddsrt_avl_cinit(&delivery_requests_td, &dc.delivery_requests);
  ddsrt_fibheap_init(&delivery_requests_fd, &dc.delivery_requests_fh);
  ddsrt_mutex_init(&dc.delivery_request_mutex);
  ddsrt_cond_init(&dc.delivery_request_cond);
  /* create the quorum listener */
  if ((dc.quorum_listener = dds_create_listener(&dc)) == NULL) {
    DDS_ERROR("failed to create quorum listener\n");
    goto err_create_quorum_listener;
  }
  dds_lset_publication_matched(dc.quorum_listener, default_durable_writer_matched_cb);
  /* create the request listener */
  if ((dc.request_listener = dds_create_listener(&dc)) == NULL) {
    DDS_ERROR("failed to create request listener\n");
    goto err_create_request_listener;
  }
  dds_lset_publication_matched(dc.request_listener, default_request_writer_matched_cb);
  if ((dc.com = dc_com_new(&dc, domainid, gv))== NULL) {
    DDS_ERROR("failed to initialize the durable client infrastructure\n");
    goto err_com_new;
  }
  activate_request_listener(&dc);
  /* start a thread to process messages coming from a ds */
  ddsrt_threadattr_init(&dc.recv_tattr);
  if ((rc = ddsrt_thread_create(&dc.recv_tid, "dc", &dc.recv_tattr, delivery_handler, &dc)) != DDS_RETCODE_OK) {
    goto err_recv_thread;
  }
  return DDS_RETCODE_OK;

err_recv_thread:
  dc_com_free(dc.com, NULL);
err_com_new:
  dds_delete_listener(dc.request_listener);
err_create_request_listener:
  dds_delete_listener(dc.quorum_listener);
err_create_quorum_listener:
  ddsrt_cond_destroy(&dc.delivery_request_cond);
  ddsrt_mutex_destroy(&dc.delivery_request_mutex);
  ddsrt_avl_cfree(&delivery_requests_td,  &dc.delivery_requests, cleanup_delivery_request);
  ddsrt_avl_cfree(&matched_request_readers_td,  &dc.matched_request_readers, cleanup_matched_request_reader);
  ddsrt_avl_cfree(&server_td,  &dc.servers, cleanup_server);
  ddsrt_free(dc.cfg.ident);
  ddsrt_atomic_dec32(&dc.refcount);
  memset(&dc, 0, sizeof(struct dc_t));
  return DDS_RETCODE_ERROR;
}

/* make sure that dc terminates when the last participant is destroyed */
static dds_entity_t _dds_durability_fini (void)
{
  dds_entity_t pp_out = 0;
  uint32_t refcount;

  /* The durable client is deinitialized when the last participant is about
   * to be removed. Note that the durable client itself also has a partition,
   * so there are in fact 2 partitions (the last "real" partition, and the durable
   * client partition. */
  refcount = ddsrt_atomic_dec32_nv(&dc.refcount);
  if (refcount != 2) {
    /* skip */
     return pp_out;
  }
  if (dc.com) {
    /* indicate the the durable client is terminating */
    ddsrt_atomic_st32 (&dc.termflag, 1);
    /* force the dc thread to terminate */
    dds_return_t rc;
    if ((rc = dds_waitset_set_trigger(dc.com->ws, true)) < 0) {
      DDS_ERROR("failed to trigger dc recv thread [%s]", dds_strretcode(rc));
    }
    /* wait until the recv thread is terminated */
    if ((rc = ddsrt_thread_join(dc.recv_tid, NULL)) < 0) {
      DDS_ERROR("failed to join the dc recv thread [%s]", dds_strretcode(rc));
    }
    dds_delete_listener(dc.request_listener);
    dds_delete_listener(dc.quorum_listener);
    dc_com_free(dc.com, &pp_out); // Participant must be deleted outside the plugin.
    ddsrt_avl_cfree(&delivery_ctx_td,  &dc.delivery_ctxs, cleanup_delivery_ctx);
    ddsrt_cond_destroy(&dc.delivery_request_cond);
    ddsrt_mutex_destroy(&dc.delivery_request_mutex);
    ddsrt_avl_cfree(&matched_request_readers_td,  &dc.matched_request_readers, cleanup_matched_request_reader);
    ddsrt_avl_cfree(&delivery_requests_td,  &dc.delivery_requests, cleanup_delivery_request);
    ddsrt_avl_cfree(&server_td,  &dc.servers, cleanup_server);
    ddsrt_free(dc.cfg.ident);
  }
  return pp_out;
}

static bool dds_durability_is_terminating (void)
{
  return (ddsrt_atomic_ld32(&dc.termflag) > 0);
}

/* Returns TRUE if the entity is an application entity, false otherwise */
static bool is_application_entity (struct dc_t *dc, dds_entity_t entity)
{
  dds_qos_t *pqos;
  dds_entity_t participant;
  dds_return_t rc;
  char *ident = dc->cfg.ident;
  void *userdata;
  size_t size = 0;
  bool result = true;

  /* by convention, if there is no ident every entity is considered a local entity */
  if (ident == NULL) {
    return true;
  }
  pqos = dds_create_qos();
  /* get the entity's participant, and determine if ident is present in the userdata */
  if ((participant = dds_get_participant(entity)) < 0) {
    DDS_ERROR("dds_get_participant failed [%s]\n", dds_strretcode(participant));
    goto err_get_participant;
  }
  if ((rc = dds_get_qos(participant, pqos)) < 0) {
    DDS_ERROR("Failed to get participant qos [%s]\n", dds_strretcode(rc));
    goto err_get_participant_qos;
  }
  if (!dds_qget_userdata(pqos, &userdata, &size)) {
    DDS_ERROR("Unable to retrieve the participant's user data");
    goto err_qget_userdata;
  }
  if ((size != strlen(ident)) || (userdata == NULL) || (strcmp(userdata, ident) != 0)) {
    /* the user data of the participant of the entity does not contain the ident,
     * so the entity is an application entity */
    result = true;
  } else {
    /* the entity resides on a DS */
    result = false;
  }
  dds_free(userdata);
  dds_delete_qos(pqos);
  return result;

err_qget_userdata:
err_get_participant_qos:
err_get_participant:
  dds_delete_qos(pqos);
  return true;
}

static dds_return_t dds_durability_new_local_reader (dds_entity_t reader, struct dds_rhc *rhc)
{
  dds_durability_kind_t dkind;
  dds_qos_t *qos;
  dds_return_t rc = DDS_RETCODE_ERROR;

  /* check if the reader is a durable reader from a "real" user application,
   * if so, send a request to obtain historical data */
  assert(reader);
  qos = dds_create_qos();
  if ((rc = dds_get_qos(reader, qos)) < 0) {
    DDS_ERROR("failed to get qos from reader [%s]\n", dds_strretcode(rc));
    goto err_get_qos;
  }
  if (!dds_qget_durability(qos, &dkind)) {
    DDS_ERROR("failed to retrieve durability qos\n");
    goto err_qget_durability;
  }
  if ((dkind == DDS_DURABILITY_TRANSIENT) || (dkind == DDS_DURABILITY_PERSISTENT)) {
    if (is_application_entity(&dc, reader)) {
      /* The user data of the participant for this durable reader does
       * not contain the ident, so the endpoint is not from a remote DS.
       * We can now publish a dc_request for this durable reader. The
       * dc_request is published as a transient-local topic. This means
       * that a late joining DS will receive the DS as long as the request
       * is not yet disposed. */
      dc_send_request_for_reader(&dc, reader, rhc);
    }
  }
  dds_delete_qos(qos);
  return rc;

err_qget_durability:
err_get_qos:
  dds_delete_qos(qos);
  return rc;
}

/* check if the writer is a durable application writer, and if so, we need to keep track of the quorum */
static dds_return_t dds_durability_new_local_writer (dds_entity_t writer)
{
  dds_durability_kind_t dkind;
  dds_qos_t *wqos;
  dds_return_t rc ;
  dds_guid_t wguid;
  char id_str[37];

  /* We only need to apply quorum checking for durable application writers.
   * Writers created by a DS (if any) do not have to be subjected to
   * quorum checking */
  if ((rc = dds_get_guid(writer, &wguid)) != DDS_RETCODE_OK) {
    DDS_ERROR("failed to retrieve writer guid\n");
    goto err_get_guid;
  }
  wqos = dds_create_qos();
  if ((rc = dds_get_qos(writer, wqos)) < 0) {
    DDS_ERROR("failed to get qos from writer \"%s\" [%s]\n", dc_stringify_id(wguid.v, id_str), dds_strretcode(rc));
    goto err_get_qos;
  }
  if (!dds_qget_durability(wqos, &dkind)) {
    DDS_ERROR("failed to retrieve durability qos for writer \"%s\"\n", dc_stringify_id(wguid.v, id_str));
    goto err_qget_durability;
  }
  /* not a durable writer, no quorum checking required */
  if ((dkind != DDS_DURABILITY_TRANSIENT) && (dkind != DDS_DURABILITY_PERSISTENT)) {
    goto skip;
  }
  /* not an application writer, no quorum checking required */
  if (!is_application_entity(&dc, writer)) {
    goto skip;
  }
  assert(dc.quorum_listener);
 /* The writer is a durable application writer, so subjected to reaching
  * a quorum before it can start publishing. We set a publication_matched
  * listener on the writer. Each time a matching durable data container is
  * discovered the listener will be triggered, causing relevant quora to
  * be updated accordingly.
  *
  * Note:
  * - setting a publication_matched listener implies that we do NOT
  *   allow that user applications can set a listener on durable writers.
  *   This is currently a limitation.
  * - be aware that the same readers can be present in the list provided
  *   by dds_get_matched_subscriptions(), and can also be triggered by the listener.
  *   Avoid counting these readers twice! */
  if ((rc = dds_set_listener(writer, dc.quorum_listener)) < 0) {
    DDS_ERROR("Unable to set the quorum listener on writer \"%s\"\n", dc_stringify_id(wguid.v, id_str));
    goto err_set_listener;
  }
  dc_check_quorum_reached(&dc, writer, true);
skip:
  dds_delete_qos(wqos);
  return DDS_RETCODE_OK;

err_set_listener:
err_qget_durability:
err_get_qos:
  dds_delete_qos(wqos);
err_get_guid:
  return rc;
}

/* Retrieve the quorum_reached value from the dds_writer that corresponds to the writer entity */
static dds_return_t dds_durability_get_quorum_reached (dds_entity_t writer, bool *quorum_reached, ddsrt_mtime_t *timeout)
{
  dds_return_t ret = DDS_RETCODE_OK;
  dds_writer *wr;
  ddsrt_mtime_t tnow;

  *quorum_reached = false;
  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK) {
    goto err_writer_lock;
  }
  /* determine the timeout lazily */
  if (timeout->v == DDS_TIME_INVALID) {
    tnow = ddsrt_time_monotonic ();
    *timeout = ddsrt_mtime_add_duration (tnow,wr->m_wr->xqos->reliability.max_blocking_time);
  }
  /* retrieve quorum reached; by default true if quorum is 0 */
  *quorum_reached = wr->quorum_reached | (dc.cfg.quorum == 0);
  dds_writer_unlock(wr);
err_writer_lock:
  return ret;
}

static dds_return_t dds_durability_wait_for_historical_data (dds_entity_t reader, dds_duration_t max_wait)
{
  dds_return_t ret = DDS_RETCODE_OK;
  dds_entity *e;
  dds_reader *rd;

  /* LH: The reader is pinned while waiting for wfhd.
   * The reason for this is that I don't want the reader to be
   * deleted while waiting for historical data.
   * I am not sure if pinning alone is sufficient. */
  if ((ret = dds_entity_pin(reader, &e)) != DDS_RETCODE_OK) {
    return ret;
  }
  rd = (dds_reader *) e;
  ddsrt_mutex_lock(&rd->wfhd_mutex);
  if (!ddsrt_cond_waitfor(&rd->wfhd_cond, &rd->wfhd_mutex, max_wait)) {
    ret = DDS_RETCODE_TIMEOUT;
  }
  ddsrt_mutex_unlock(&rd->wfhd_mutex);
  dds_entity_unpin(e);
  return ret;
}

/* This function waits for a quorum of durable data containers to be available,
 * or timeout in case max_blocking_time is expired and quorum not yet reached.
 *
 * If the quorum is not reached before the max_blocking_time() is expired,
 * DDS_RETCODE_PRECONDITION is returned
 *
 * The quorum is calculated based on the number of discovered and matched data containers
 * for a writer. This also implies that if a writer has reached a quorum,
 * some other writer may not have reached the quorum yet.
 *
 * return
 *   DDS_RETCODE_OK             if quorum is reached
 *   DDS_PRECONDITION_NOT_MET   otherwise
 */
static dds_return_t dds_durability_wait_for_quorum (dds_entity_t writer)
{
  dds_return_t ret = DDS_RETCODE_ERROR;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();
  ddsrt_mtime_t timeout;
  dds_duration_t tdur;
  bool quorum_reached;

  /* Check if the quorum for a durable writer is reached.
   * If not, we will head bang until the quorum is reached.
   * To prevent starvation we use a 10ms sleep in between successive headbangs.
   * When the quorum is reached within the max_blocking_time,
   * DDS_RETCODE_OK is returned, otherwise DDS_PRECONDITION_NOT_MET
   * is returned. The max_blocking_time itself is retrieved lazily
   * on the first call to dds_durability_get_quorum_reached().
   *
   * LH: A better solution would be to wait on a condition variable,
   * and get notified once the quorum is reached (or the max_blocking_time
   * times out) */
  timeout.v = DDS_TIME_INVALID;
  do {
    if ((ret = dds_durability_get_quorum_reached(writer, &quorum_reached, &timeout)) != DDS_RETCODE_OK) {
      break;
    }
    if (quorum_reached) {
      ret = DDS_RETCODE_OK;
      break;
    }
    tnow = ddsrt_time_monotonic();
    if (tnow.v >= timeout.v) {
      ret = DDS_RETCODE_PRECONDITION_NOT_MET;
      break;
    }
    tdur = (timeout.v -tnow.v <= DDS_MSECS(10)) ? timeout.v - tnow.v : DDS_MSECS(10);
    dds_sleepfor (tdur);
  } while (true);  /* Note: potential but deliberate infinite loop when max_blocking_time is set to DDS_INFINITY. */
  return ret;
}

/* get the configured quorum */
static uint32_t dds_durability_get_quorum (void)
{
  return dc.cfg.quorum;
}

void dds_durability_creator (dds_durability_t *dc)
{
  dc->dds_durability_init = dds_durability_init;
  dc->_dds_durability_fini = _dds_durability_fini;
  dc->dds_durability_get_quorum = dds_durability_get_quorum;
  dc->dds_durability_new_local_reader = dds_durability_new_local_reader;
  dc->dds_durability_new_local_writer = dds_durability_new_local_writer;
  dc->dds_durability_wait_for_quorum = dds_durability_wait_for_quorum;
  dc->dds_durability_is_terminating = dds_durability_is_terminating;
  dc->dds_durability_wait_for_historical_data = dds_durability_wait_for_historical_data;
}
