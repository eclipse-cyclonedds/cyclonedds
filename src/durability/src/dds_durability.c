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
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/durability/dds_durability.h"
#include "dds/durability/durablesupport.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/log.h"
#include "ddsc/dds.h"
#include <string.h>
#include "dds__writer.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsrt/avl.h"

#define DEFAULT_QOURUM                       1
#define DEFAULT_IDENT                        "durable_support"


#define TRACE(...) DDS_CLOG (DDS_LC_DUR, &domaingv->logconfig, __VA_ARGS__)

static char *dc_stringify_id(const DurableSupport_id_t id, char *buf)
{
  assert(buf);
  snprintf (buf, 37, "%02x%02x%02x%02x\055%02x%02x\055%02x%02x\055%02x%02x\055%02x%02x%02x%02x%02x%02x",
                id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7],
                id[8], id[9], id[10], id[11], id[12], id[13], id[14], id[15]);
  return buf;
}

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

/* This represents a request for historical data. */
struct pending_request_t {
  ddsrt_avl_node_t node;  /* represents a node in the tree of pending requests */
  ddsrt_fibheap_node_t fhnode;  /* represents a node in the priority queue */
  dds_instance_handle_t ih; /* the instance handle that represents the reader */
  uint64_t seq; /* the sequence number of the request; this is used as the key */
  dds_entity_t reader; /* the reader that does the request */
  struct dds_rhc *rhc; /* reader rhc */
  dds_duration_t exptime; /* expiry time for this pending request */
};

static int cmp_pending_request (const void *a, const void *b)
{
  uint64_t *s1 = (uint64_t *)a;
  uint64_t *s2 = (uint64_t *)b;

  return (*s1 < *s2) ? -1 : ((*s1 > *s2) ? 1 : 0);
}

static void cleanup_pending_request (void *n)
{
  struct pending_request_t *pr = (struct pending_request_t *)n;
  ddsrt_free(pr);
}

static int cmp_exp_time (const void *a, const void *b)
{
  struct pending_request_t *pr1 = (struct pending_request_t *)a;
  struct pending_request_t *pr2 = (struct pending_request_t *)b;

  return (pr1->exptime < pr2->exptime) ? -1 : ((pr1->exptime > pr2->exptime) ? 1 : 0);
}

static const ddsrt_avl_ctreedef_t pending_requests_td = DDSRT_AVL_CTREEDEF_INITIALIZER(offsetof (struct pending_request_t, node), offsetof (struct pending_request_t, seq), cmp_pending_request, 0);

static const ddsrt_fibheap_def_t pending_requests_fd = DDSRT_FIBHEAPDEF_INITIALIZER(offsetof (struct pending_request_t, fhnode), cmp_exp_time);

/* Administration to keep track of the request readers of a ds
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
  ddsrt_avl_ctree_t pending_requests; /* tree containing pending requests  */
  ddsrt_fibheap_t pending_requests_fh; /* priority queue for pending requests, prioritized by expiry time */
  dds_instance_handle_t selected_request_reader_ih; /* instance handle to matched request reader, DDS_HANDLE_NIL if not available */
  ddsrt_avl_ctree_t matched_request_readers; /* tree containing the request readers on a ds that match with my ds writer */
  uint32_t nr_of_matched_dc_requests; /* indicates the number of matched dc_request readers for the dc_request writer of this client */
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
  void *samples[1] = { NULL };
  dds_sample_info_t info[1];
  void *userdata;
  size_t size = 0;
  char id_str[37];
  bool result = false;

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
  (void)dds_return_loan (com->rd_participant, samples, rc);
  return result;

err_lookup_instance:
err_read_instance:
  (void)dds_return_loan (com->rd_participant, samples, rc);
err_qget_userdata:
  return false;
}

/* Determine if the quorum for the writer is reached or not.
 * This function also does its job when the quorum threshold is 0,
 * even though this case is likely to be prohibited because it
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
     * size of the list beforehand, we dynamically extend the list
     * until it is large enough to hold all handles of matching readers. */
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
  if ((com->tp_response = dds_create_topic (com->participant, &DurableSupport_bead_desc, "dc_response", tqos, NULL)) < 0) {
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
  /* create waitset and attach read conditions */
  if ((com->ws = dds_create_waitset(com->participant)) < 0) {
    DDS_ERROR("failed to create dc waitset [%s]\n", dds_strretcode(-com->ws));
    goto err_waitset;
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
  dds_delete(com->ws);
err_waitset:
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

static void dc_com_free (struct com_t *com)
{
  assert(com);
  DDS_CLOG(DDS_LC_DUR, &dc.gv->logconfig, "destroying dc infrastructure\n");
  dds_delete(com->participant);
  ddsrt_free(com);
  dc.com = NULL;
  return;
}

static dds_return_t dc_com_request_write (struct com_t *com, dds_entity_t reader, uint64_t seq)
{
  /* check for publication_matched() on the dc_request writer to make sure that the request arrives */
  /* create a request */
  /* set the request on the pending liust */
  /* send the request */

  /* note: we allow doing a request for volatile readers */
  DurableSupport_request *request;
  dds_qos_t *rqos;
  dds_return_t ret = DDS_RETCODE_OK;
  uint32_t plen, i;
  char **partitions;

  if ((rqos = dds_create_qos()) == NULL) {
    DDS_ERROR("Unable to create reader qos for dc_request");
    goto err_alloc_rqos;
  }
  if ((ret = dds_get_qos(reader, rqos)) != DDS_RETCODE_OK) {
    DDS_ERROR("Unable to get reader qos for dc_request");
    goto err_get_qos;
  }
  if (!dds_qget_partition(rqos, &plen, &partitions)) {
    DDS_ERROR("Failed to retrieve partition qos for dc_request");
    goto err_qget_partition;
  }
  request = DurableSupport_request__alloc();
  memcpy(request->requestid.client, dc.cfg.id, 16);
  request->requestid.seq = seq;
  request->partitions._length = plen;
  request->partitions._maximum = plen;
  request->partitions._buffer = dds_sequence_string_allocbuf(plen);
  request->partitions._release = true;
  for (i=0; i < plen; i++) {
    request->partitions._buffer[i] = ddsrt_strdup(partitions[i]);
  }
  request->timeout = DDS_SECS(5);
  if ((ret = dds_write(com->wr_request, request)) < 0) {
    DDS_ERROR("failed to publish dc_request [%s]", dds_strretcode(-ret));
    goto err_request_write;
  }
  DDS_CLOG(DDS_LC_DUR, &dc.gv->logconfig, "publish dc_request {\"client\":\"%s\", \"seq\":%" PRIu64 "}\n", dc.cfg.id_str, seq);
err_request_write:
  dc_free_partitions(plen, partitions);
  dds_delete_qos(rqos);
  DurableSupport_request_free(request, DDS_FREE_ALL);
  return ret;

err_qget_partition:
err_get_qos:
  dds_delete_qos(rqos);
err_alloc_rqos:
  return DDS_RETCODE_ERROR;
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

  void *samples[MAX_SAMPLES] = { NULL };
  dds_sample_info_t info[MAX_SAMPLES];
  int samplecount;
  int j;

  /* dds_read/take allocates memory for the data if samples[0] is a null pointer.
   * The memory must be released when done by returning the loan */
  samples[0]= NULL;
  samplecount = dds_take_mask (rd, samples, info, MAX_SAMPLES, MAX_SAMPLES, DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
  if (samplecount < 0) {
    DDS_ERROR("durable client failed to take ds_status [%s]", dds_strretcode(-samplecount));
    goto err_samplecount;
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
  }
  (void)dds_return_loan (rd, samples, samplecount);
err_samplecount:
  return samplecount;
#undef MAX_SAMPLES
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
     * Check if the quorem has been lost */
    dc_check_quorum_reached(dc, writer, false);
  }
err_last_subscription_handle:
  return;
}

/* publish a reader request
 * The reader request is published as a transient-local topic which is
 * keyed by the guid of this client and a monotonically increasing sequence number.
 * Even though this is not recommended, this allows multiple requests from the
 * same reader (because they have a different sequence number).
 *
 * For each outgoing request, a pending request entry will be created to administrate
 * the outgoing requests. Pending requests can have a expiration. Expired pending
 * requests will lead to removal of the pending request, and a dispose of the
 * corresponding reader request to prevent that late joining
 */
static struct pending_request_t *dc_publish_reader_request (struct dc_t *dc, dds_entity_t reader, struct dds_rhc *rhc)
{
  struct pending_request_t *pr;
  dds_instance_handle_t ih;
  dds_return_t rc;

  /* verify if the reader still exists; if not, delete the request */
  if ((rc = dds_get_instance_handle(reader, &ih)) != DDS_RETCODE_OK) {
    DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "No need to publish dc_request, reader is not available\n");
    return NULL;
  }
  /* create a pending request entry and insert it in the table of pending requests */
  if ((pr = (struct pending_request_t *)ddsrt_malloc(sizeof(struct pending_request_t))) == NULL) {
    DDS_ERROR("Failed to allocate a reader request\n");
    goto err_alloc_reader_request;
  }
  memset(pr, 0, sizeof(struct pending_request_t));
  pr->ih = ih;
  pr->seq = ++(dc->seq);
  pr->reader = reader;
  pr->rhc = rhc;
  ddsrt_avl_cinsert(&pending_requests_td, &dc->pending_requests, pr);
  /* publish the request  */
  if ((rc = dc_com_request_write(dc->com, reader, dc->seq)) != DDS_RETCODE_OK) {
    DDS_ERROR("Failed to publish dc_request\n");
    goto err_publish_reader_request;
  }
  return pr;

err_publish_reader_request:
  ddsrt_avl_cdelete(&pending_requests_td, &dc->pending_requests, pr);
  cleanup_pending_request(pr);
  --(dc->seq);
err_alloc_reader_request:
  return NULL;
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
      /* A dc_request reader on a data container has matched with the dc_request writer.
       * From now on it is allowed to publish requests.
       * In case there are any pending requests, we can sent them now */
      dc_add_matched_request_reader(dc, ep, ih);
    }
    dds_builtintopic_free_endpoint(ep);
  } else {
    /* the dc_request endpoint is not available any more. */
    dc_remove_matched_request_reader(dc, ih);
  }
}

static uint32_t recv_handler (void *a)
{
  struct dc_t *dc = (struct dc_t *)a;
  dds_duration_t timeout = DDS_INFINITY;
  dds_attach_t wsresults[1];
  size_t wsresultsize = sizeof(wsresults)/sizeof(wsresults[0]);
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
        }
      }
    }
  }
  DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "stop durable client thread\n");
  return 0;
}

/* set the request listener to learn about the existence of matching request readers.
 * Because matches may have occurred already before */

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
         * In case there are any pending requests, we can sent them now */
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

dds_return_t dds_durability_init (const dds_domainid_t domainid, struct ddsi_domaingv *gv)
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
  ddsrt_avl_cinit(&server_td, &dc.servers);
  ddsrt_avl_cinit(&pending_requests_td, &dc.pending_requests);
  ddsrt_avl_cinit(&matched_request_readers_td, &dc.matched_request_readers);
  ddsrt_fibheap_init(&pending_requests_fd, &dc.pending_requests_fh);
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
  if ((rc = ddsrt_thread_create(&dc.recv_tid, "dc", &dc.recv_tattr, recv_handler, &dc)) != DDS_RETCODE_OK) {
    goto err_recv_thread;
  }
  return DDS_RETCODE_OK;

err_recv_thread:
  dc_com_free(dc.com);
err_com_new:
  dds_delete_listener(dc.request_listener);
err_create_request_listener:
  dds_delete_listener(dc.quorum_listener);
err_create_quorum_listener:
  ddsrt_free(dc.cfg.ident);
  return DDS_RETCODE_ERROR;
}

/* make sure that dc terminates when the last participant is destroyed */
dds_return_t dds_durability_fini (void)
{
  dds_return_t rc;
  uint32_t refcount;

  /* The durable client is deinitialized when the last participant is about
   * to be removed. Note that the durable client itself also has a partition,
   * so there are in fact 2 partitions (the last "real" partition, and the durable
   * client partition. */
  refcount = ddsrt_atomic_dec32_nv(&dc.refcount);
  if (refcount != 2) {
    /* skip */
     return DDS_RETCODE_OK;
  }
  if (dc.com) {
    /* indicate the the durable client is teminating */
    ddsrt_atomic_st32 (&dc.termflag, 1);
    /* force the dc thread to terminate */
    if ((rc = dds_waitset_set_trigger(dc.com->ws, true)) < 0) {
      DDS_ERROR("failed to trigger dc recv thread [%s]", dds_strretcode(rc));
    }
    /* wait until the recv thread is terminated */
    if ((rc = ddsrt_thread_join(dc.recv_tid, NULL)) < 0) {
      DDS_ERROR("failed to join the dc recv thread [%s]", dds_strretcode(rc));
    }
    dds_delete_listener(dc.request_listener);
    dds_delete_listener(dc.quorum_listener);
    dc_com_free(dc.com);
    ddsrt_avl_cfree(&matched_request_readers_td,  &dc.matched_request_readers, cleanup_matched_request_reader);
    ddsrt_avl_cfree(&pending_requests_td,  &dc.pending_requests, cleanup_pending_request);
    ddsrt_avl_cfree(&server_td,  &dc.servers, cleanup_server);
    ddsrt_free(dc.cfg.ident);
  }
  return DDS_RETCODE_OK;
}

bool dds_durability_is_terminating (void)
{
  return (ddsrt_atomic_ld32(&dc.termflag) > 0);
}

dds_return_t dds_durability_new_local_reader (dds_entity_t reader, struct dds_rhc *rhc)
{
  dds_durability_kind_t dkind;
  dds_qos_t *qos, *parqos;
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
    dds_entity_t par;
    void *userdata;
    size_t size = 0;

    /* Creation of a durable reader must only lead to the publication
     * of a dc_request when the reader is a "real" application reader.
     * If the reader belongs to a participant that carries user data
     * containing the IDENT, then the reader is a data container.
     * We don't need to generate dc_requests for data containers */
    if ((par = dds_get_participant(reader)) < 0) {
      DDS_ERROR("Unable to retrieve the participant of the reader [%s]\n", dds_strretcode(rc));
      goto err_get_participant;
    }
    if ((parqos = dds_create_qos()) == NULL) {
      DDS_ERROR("Failed to allocate qos\n");
      goto err_alloc_parqos;
    }
    if ((rc = dds_get_qos(par, parqos)) < 0) {
      DDS_ERROR("Failed to get topic qos [%s]", dds_strretcode(rc));
      goto err_get_parqos;
    }
    if (!dds_qget_userdata(parqos, &userdata, &size)) {
      DDS_ERROR("Unable to retrieve the participant's user data for reader\n");
      goto err_qget_userdata;
    }
    if ((size != strlen(IDENT)) || (userdata == NULL)  || (strcmp(userdata, IDENT) != 0)) {
      /* the user data of the participant for this durable reader does
       * not contain the ident, so the endoint is not from a remote DS.
       * We can now publish a dc_request for this durable reader. The
       * dc_request is published as a transient-local topic. This means
       * that a late joining DS will receive the DS as long as the request
       * is not yet disposed.*/
      (void)dc_publish_reader_request(&dc, reader, rhc);
    }
    dds_free(userdata);
    dds_delete_qos(parqos);
  }
  dds_delete_qos(qos);
  return rc;

err_qget_userdata:
err_get_parqos:
  dds_delete_qos(parqos);
err_alloc_parqos:
err_get_participant:
err_qget_durability:
err_get_qos:
  dds_delete_qos(qos);
  return rc;
}

dds_return_t dds_durability_new_local_writer (dds_entity_t writer)
{
  dds_durability_kind_t dkind;
  dds_qos_t *qos;
  dds_return_t rc = DDS_RETCODE_ERROR;
  dds_guid_t wguid;
  char id_str[37];

  /* check if the writer is a durable writer, and if so, we need to keep track of the quorum */
  assert(writer);
  qos = dds_create_qos();
  if ((rc = dds_get_qos(writer, qos)) < 0) {
    DDS_ERROR("failed to get qos from writer [%s]\n", dds_strretcode(rc));
    goto err_get_qos;
  }
  if (!dds_qget_durability(qos, &dkind)) {
    DDS_ERROR("failed to retrieve durability qos");
    goto err_qget_durability;
  }
  if ((dkind == DDS_DURABILITY_TRANSIENT) || (dkind == DDS_DURABILITY_PERSISTENT)) {
    assert(dc.quorum_listener);
    /* The writer is durable, so subjected to reaching a quorum before
     * it can start publishing. We set a publication_matched listener on
     * the writer. Each time a matching durable data container is discovered
     * the listener will be triggered, causing relevant quora to be updated
     * accordingly.
     *
     * Note that setting a publication_matched listener implies that we do NOT
     * allow that user application can set a listener on durable writers.
     * This is currently a limitation. */
    if ((rc = dds_get_guid(writer, &wguid)) != DDS_RETCODE_OK) {
      DDS_ERROR("failed to retrieve writer guid");
      goto err_get_guid;
    }
    /* We now set a quorum listener on the durable writer.
     * Each time a matching reader will appear, wewill get notified.
     * Existing readers may already have matched before the listener takes effect,
     * so these publication_match events may be missed. Luckily, we can request
     * all matching readers using dds_get_matched_subscriptions().
     *
     * Note: be aware that the same readers can be present in the list provided
     * by dds_get_matched_subscriptions(), and can also be triggered by the listener.
     * Avoid counting these readers twice!
     */
    DDS_CLOG(DDS_LC_DUR, &dc.gv->logconfig, "durable writer \"%s\" subject to quorum checking\n", dc_stringify_id(wguid.v, id_str));
    if ((rc = dds_set_listener(writer, dc.quorum_listener)) < 0) {
      DDS_ERROR("Unable to set the quorum listener on writer \"%s\"\n", dc_stringify_id(wguid.v, id_str));
      goto err_set_listener;
    }
    dc_check_quorum_reached(&dc, writer, true);
  }
  dds_delete_qos(qos);
  return DDS_RETCODE_OK;

err_set_listener:
err_get_guid:
err_qget_durability:
err_get_qos:
  dds_delete_qos(qos);
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

/* This function waits for a quorum of durable data containers to be available,
 * or timeout in case max_blocking_time is expired and quorum not yet reached.
 *
 * If the quorum is not reached before the max_blocking_time() is expired,
 * DDS_RETCODE_PRECONDITION
 *
 * The quorum is calculated based on the number of discovered and matched data containers
 * for a writer. This also implies that if a writer has reached a quorum,
 * some other writer may not have reached the quorum yet.
 *
 * return
 *   DDS_RETCODE_OK             if quorum is reached
 *   DDS_PRECONDITION_NOT_MET   otherwise
 */
dds_return_t dds_durability_wait_for_quorum (dds_entity_t writer)
{
  dds_return_t ret = DDS_RETCODE_ERROR;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();
  ddsrt_mtime_t timeout;
  dds_duration_t tdur;
  bool quorum_reached;

  /* Check if the quorum for a durable writer is reached.
   * If not, we will head bang until the quorum is reached.
   * To prevent starvation we use a 10ms sleep in between.
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
uint32_t dds_durability_get_quorum (void)
{
  return dc.cfg.quorum;
}

