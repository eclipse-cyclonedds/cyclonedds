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
#include "dds/durability/client_durability.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/log.h"
#include "ddsc/dds.h"
#include <string.h>
#include "dds__writer.h"
#include "dds/ddsi/ddsi_endpoint.h"

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


struct quorum_entry_key_t {
  char *partition;  /* the partition of the data container; should be a singleton */
  char *tpname;  /* topic name */
  /* LH: todo: add a type id to support xtypes */
};

struct quorum_entry_t {
  ddsrt_avl_node_t node;
  struct quorum_entry_key_t key;
  uint32_t cnt;  /* the number of data containers found for this partition/topic combination */
};

static int cmp_quorum_entry (const void *a, const void *b)
{
  struct quorum_entry_key_t *qk1 = (struct quorum_entry_key_t *)a;
  struct quorum_entry_key_t *qk2 = (struct quorum_entry_key_t *)b;
  int cmp;

  if ((cmp = strcmp(qk1->partition, qk2->partition)) < 0) {
    return -1;
  } else if (cmp > 0) {
    return 1;
  } else if ((cmp = strcmp(qk1->tpname, qk2->tpname)) < 0) {
    return -1;
  } else if (cmp > 0) {
    return 1;
  } else {
    return 0;
  }
}

static void cleanup_quorum_entry (void *n)
{
  struct quorum_entry_t *qe = (struct quorum_entry_t *)n;
  ddsrt_free(qe->key.partition);
  ddsrt_free(qe->key.tpname);
  ddsrt_free(qe);
}

static const ddsrt_avl_ctreedef_t quorum_entry_td = DDSRT_AVL_CTREEDEF_INITIALIZER(offsetof (struct quorum_entry_t, node), offsetof (struct quorum_entry_t, key), cmp_quorum_entry, 0);

struct handle_to_quorum_entry_t {
  ddsrt_avl_node_t node;
  dds_instance_handle_t ih;
  struct quorum_entry_t *qe_ref;  /* reference to quorum entry */
};

static int cmp_instance_handle (const void *a, const void *b)
{
  dds_instance_handle_t *ih1 = (dds_instance_handle_t *)a;
  dds_instance_handle_t *ih2 = (dds_instance_handle_t *)b;

  return (*ih1 < *ih2) ? -1 : ((*ih1 > *ih2) ? 1 : 0);
}

static void cleanup_handle_to_quorum_entry (void *n)
{
  struct handle_to_quorum_entry_t *hqe = (struct handle_to_quorum_entry_t *)n;

  ddsrt_free(hqe);
}

static const ddsrt_avl_ctreedef_t handle_to_quorum_entry_td = DDSRT_AVL_CTREEDEF_INITIALIZER(offsetof (struct handle_to_quorum_entry_t, node), offsetof (struct handle_to_quorum_entry_t, ih), cmp_instance_handle, 0);

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
    char *request_partition;  /* partition to send requests too; by default same as <hostname> */
    uint32_t quorum;  /*quorum of durable services needed to unblock durable writers */
    char *ident; /* ds identification */
  } cfg;
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
  ddsrt_avl_ctree_t quorum_entries; /* tree containing quora for all discovered data containers */
  ddsrt_avl_ctree_t handle_to_quorum_entries; /* tree that maps subscription handles to references to quorum entries */
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

#if 0
/* callback function to update the sequence number administration of a container
 * and to optionally monitor the contents of data containers */
static void default_data_available_cb (dds_entity_t rd, void *arg)
{
#define MAX_SAMPLES      100
  int samplecount;
  static void *samples[MAX_SAMPLES];
  dds_sample_info_t info[MAX_SAMPLES];

  (void)arg;

  samplecount = dds_read_mask (rd, samples, info, MAX_SAMPLES, MAX_SAMPLES, DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE);
  printf("LH *** samplecount = %d\n", samplecount);
  if (samplecount < 0) {
    printf("LH *** dds_read_mask for status reader failed: %s", dds_strretcode(-samplecount));
    goto samplecount_err;
  }
  for (int j = 0; j < samplecount; j++) {
    DurableSupport_status *status = (DurableSupport_status *)samples[j];
    printf("LH *** recv status(): name=%s\n", status->name);
    info[j].publication_handle();

  }
samplecount_err:
#undef MAX_SAMPLES
  return;
}
#endif

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
  /* create dc_request writer */
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
  dds_qset_durability(tqos, DDS_DURABILITY_VOLATILE);
  dds_qset_history(tqos, DDS_HISTORY_KEEP_ALL, DDS_LENGTH_UNLIMITED);
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
  /* subinfo reader and listener to detect remote data containers */
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

  static void *samples[MAX_SAMPLES];
  static dds_sample_info_t info[MAX_SAMPLES];
  int samplecount;
  int j;

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
  }
  return samplecount;
#undef MAX_SAMPLES
}

/* verifies if the user data of the endpoint contains the identifier
 * that indicates that this endpoint is a durable container */
static bool dc_is_ds_endpoint (struct com_t *com, dds_builtintopic_endpoint_t *ep, const char *ident)
{
  dds_builtintopic_endpoint_t template;
  dds_builtintopic_participant_t *participant;
  dds_instance_handle_t ih;
  dds_return_t rc;
  static void *samples[1];
  static dds_sample_info_t info[1];
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
  return result;

err_lookup_instance:
err_read_instance:
err_qget_userdata:
  return false;
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

static ddsi_guid_prefix_t dc_ddsi_hton_guid_prefix (ddsi_guid_prefix_t p)
{
  int i;
  for (i = 0; i < 3; i++)
    p.u[i] = ddsrt_toBE4u (p.u[i]);
  return p;
}

static ddsi_entityid_t dc_ddsi_hton_entityid (ddsi_entityid_t e)
{
  e.u = ddsrt_toBE4u (e.u);
  return e;
}

static ddsi_guid_t dc_ddsi_hton_guid (ddsi_guid_t g)
{
  g.prefix = dc_ddsi_hton_guid_prefix (g.prefix);
  g.entityid = dc_ddsi_hton_entityid (g.entityid);
  return g;
}

static void dc_ddsiguid2guid (dds_guid_t *dds_guid, const ddsi_guid_t *ddsi_guid)
{
  ddsi_guid_t tmp;
  tmp = dc_ddsi_hton_guid (*ddsi_guid);
  memcpy (dds_guid, &tmp, sizeof (*dds_guid));
}

static void dc_check_quorum_reached (struct dc_t *dc, dds_entity_t writer, bool new_qe)
{
  dds_qos_t *qos;
  dds_writer *wr;
  dds_return_t rc;
  uint32_t plen, i;
  char **partitions;
  struct quorum_entry_key_t key;
  struct quorum_entry_t *qe;
  bool quorum_reached = true;

  assert(writer);
  assert(dc);

  qos = dds_create_qos();
  if ((rc = dds_get_qos(writer, qos)) < 0) {
    DDS_ERROR("failed to get qos from writer [%s]", dds_strretcode(rc));
    goto err_get_qos;
  }
  if (!dds_qget_partition(qos, &plen, &partitions)) {
    DDS_ERROR("failed to get partitions from qos\n");
    goto err_qget_partition;
  }
  assert(plen > 0);
  if ((rc = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK) {
    DDS_ERROR("failed to lock writer\n");
    goto err_writer_lock;
  }
  key.tpname = ddsrt_strdup(wr->m_topic->m_name);
  if (((!wr->quorum_reached) && (new_qe)) || ((wr->quorum_reached) && (!new_qe))) {
    /* the quorum was not reached and a new quorum entry has been found, or
     * the quorum was reached and a quorum entry has been lost
     * In both cases check if the quorum still holds */
    for (i=0; i < plen && quorum_reached; i++) {
      /* lookup the quorum entry, and determine if a quorum has reached for all relevant data containers */
      key.partition = ddsrt_strdup(partitions[i]);
      if ((qe = ddsrt_avl_clookup (&quorum_entry_td, &dc->quorum_entries, &key)) == NULL)  {
        quorum_reached = false;
      } else {
        quorum_reached = (qe->cnt >= dc->cfg.quorum);
      }
      ddsrt_free(key.partition);
    }
    if (wr->quorum_reached != quorum_reached) {
      dds_guid_t guid;
      char id_str[37];

      wr->quorum_reached = quorum_reached;
      dc_ddsiguid2guid(&guid, &wr->m_entity.m_guid);
      DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "quorum for writer \"%s\" %s\n", dc_stringify_id(guid.v, id_str), quorum_reached ? "reached" : "lost");
    }
  }
  ddsrt_free(key.tpname);
  dds_writer_unlock (wr);
err_writer_lock:
  dc_free_partitions(plen, partitions);
err_qget_partition:
err_get_qos:
  dds_delete_qos(qos);
    return;
}



/* get a quorum counter for the builtin endoint and create a quorum counter for the endpoint */
static struct quorum_entry_t *dc_get_or_create_quorum_entry (struct dc_t *dc, dds_builtintopic_endpoint_t *ep)
{
  struct quorum_entry_t *qe;
  struct quorum_entry_key_t key;
  uint32_t plen;
  char **partitions;

  if (!dds_qget_partition(ep->qos, &plen, &partitions)) {
    DDS_ERROR("failed to get partitions from qos\n");
    goto err_qget_partition;
  }
  assert(plen == 1);
  key.tpname = ddsrt_strdup(ep->topic_name);
  key.partition = ddsrt_strdup(partitions[0]);
  if ((qe = ddsrt_avl_clookup (&quorum_entry_td, &dc->quorum_entries, &key)) == NULL)  {
    /* create quorum entry */
    if ((qe = (struct quorum_entry_t *)ddsrt_malloc(sizeof(struct quorum_entry_t))) == NULL) {
      DDS_ERROR("failed to allocate quorum entry\n");
      goto err_alloc_quorum_entry;
    }
    qe->key.partition = ddsrt_strdup(key.partition);
    qe->key.tpname = ddsrt_strdup(key.tpname);
    qe->cnt = 0;
    ddsrt_avl_cinsert(&quorum_entry_td, &dc->quorum_entries, qe);
    DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "quorum entry for \"%s.%s\" created\n", qe->key.partition, qe->key.tpname);
  }
  ddsrt_free(key.tpname);
  ddsrt_free(key.partition);
  dc_free_partitions(plen, partitions);
  return qe;

err_alloc_quorum_entry:
  ddsrt_free(key.tpname);
  ddsrt_free(key.partition);
  dc_free_partitions(plen, partitions);
err_qget_partition:
  return NULL;
}

static struct quorum_entry_t *dc_link_handle_to_quorum_entry (struct dc_t *dc, dds_instance_handle_t ih, struct quorum_entry_t *qe)
{
  struct handle_to_quorum_entry_t *hqe;

  assert(qe);
  /* link the handle to the quorum entry, so we can lookup the
   * quorum entry if the data container identified by the handle
   * disappears */
  if ((hqe = (struct handle_to_quorum_entry_t *)ddsrt_malloc(sizeof(struct handle_to_quorum_entry_t))) == NULL) {
    DDS_ERROR("failed to allocate handle_to_quorum_entry_t\n");
    goto err_alloc_hqe;
  }
  hqe->ih = ih;
  hqe->qe_ref = qe;
  ddsrt_avl_cinsert(&handle_to_quorum_entry_td, &dc->handle_to_quorum_entries, hqe);
  qe->cnt++;
  DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "quorum for \"%s.%s\" bumped to %" PRIu32 " (ih %" PRIx64 ")\n", qe->key.partition, qe->key.tpname, qe->cnt, ih);
  return qe;

err_alloc_hqe:
  return NULL;
}

/* Update the quorum entry when a data container leaves.
 * Returns a reference to the quorum entry as long as there are still containers, or NULL otherwise */
static struct quorum_entry_t *dc_unlink_handle_to_quorum_entry (struct dc_t *dc, dds_instance_handle_t ih)
{
  struct handle_to_quorum_entry_t *hqe;
  struct quorum_entry_t *qe = NULL;
  ddsrt_avl_dpath_t dpath_hqe, dpath_qe;

  /* look up the quorum entry via the instance handle */
  if ((hqe = ddsrt_avl_clookup_dpath (&handle_to_quorum_entry_td, &dc->handle_to_quorum_entries, &ih, &dpath_hqe)) != NULL) {
    qe = hqe->qe_ref;
    assert(qe);
    assert(qe->cnt > 0);
    qe->cnt--;
    DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "quorum for \"%s.%s\" decreased to %" PRIu32 " (ih %" PRIx64 ")\n", qe->key.partition, qe->key.tpname, qe->cnt, ih);
    if (qe->cnt == 0) {
      /* by unlinking this subscription info, the last reference to the quorum entry is now gone.
       * We can now garbage collect the quorum entry itself.  */
      DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "quorum entry for \"%s.%s\" deleted\n", qe->key.partition, qe->key.tpname);
      if (ddsrt_avl_clookup_dpath(&quorum_entry_td, &dc->quorum_entries, &qe->key, &dpath_qe)) {
        ddsrt_avl_cdelete_dpath(&quorum_entry_td, &dc->quorum_entries, qe, &dpath_qe);
        cleanup_quorum_entry(qe);
        qe = NULL;
      }
    }
    ddsrt_avl_cdelete_dpath(&handle_to_quorum_entry_td, &dc->handle_to_quorum_entries, hqe, &dpath_hqe);
    cleanup_handle_to_quorum_entry(hqe);
  }
  return qe;
}

/* called when there is a match for a durable writer */
static void default_durable_writer_matched_cb (dds_entity_t writer, dds_publication_matched_status_t status, void *arg)
{
  struct dc_t *dc = (struct dc_t *)arg;
  dds_instance_handle_t ih;
  dds_builtintopic_endpoint_t *ep;
  struct quorum_entry_t *qe;

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
      /* a data container has matched with this writer.
       * Increase the quorum counter for this container.
       * If no quorum counter existed, then create one.
       * Also link the subscription handle to the quorum counter,
       * so that we can decrease the quorum counter in case the
       * data container does not exist. */
      if ((qe = dc_get_or_create_quorum_entry(dc, ep)) == NULL) {
        goto err_inc_or_create_quorum_entry;
      }
      dc_link_handle_to_quorum_entry(dc, ih, qe);
      /* a relevant data container from a durable service for this writer has become available.
       * Reevaluate if the quorum has been reached */
      dc_check_quorum_reached(dc, writer, true);
    }
    dds_builtintopic_free_endpoint(ep);
  } else {
    /* the endpoint that represents a data container is not available any more.
     * decrease the quorum counter for the data container associated with
     * the handle. If the quorum counter reaches 0, we'll garbage collect
     * the quorum counter entry. */
    qe = dc_unlink_handle_to_quorum_entry(dc, ih);
    /* a data container from a durable service has been lost.
     * reevaluate if the quorum still holds */
    dc_check_quorum_reached(dc, writer, false);
  }
  err_inc_or_create_quorum_entry:
err_last_subscription_handle:
  return;
}

static uint32_t recv_handler (void *a)
{
  struct dc_t *dc = (struct dc_t *)a;
  dds_duration_t timeout = DDS_INFINITY;
  dds_attach_t wsresults[1];
  size_t wsresultsize = sizeof(wsresults)/sizeof(wsresults[0]);
  int n, j;
  dds_return_t rc;

  if ((dc->quorum_listener = dds_create_listener(dc)) == NULL) {
    DDS_ERROR("failed to create quorum listener\n");
    goto err_create_quorum_listener;
  }
  dds_lset_publication_matched(dc->quorum_listener, default_durable_writer_matched_cb);
  if ((dc->subinfo_listener = dds_create_listener(dc)) == NULL) {
    DDS_ERROR("failed to create subinfo listener\n");
    goto err_create_subinfo_listener;
  }
  // dds_lset_data_available(dc->subinfo_listener, default_evaluate_quorum_cb);
  if ((rc = dds_set_listener(dc->com->rd_subinfo, dc->subinfo_listener)) < 0) {
    DDS_ERROR("Unable to set the subinfo listener\n");
    goto err_set_listener;
  }
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
  dds_delete_listener(dc->subinfo_listener);
  dc->subinfo_listener = NULL;
  dds_delete_listener(dc->quorum_listener);
  dc->quorum_listener = NULL;
  DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "stop durable client thread\n");
  return 0;

err_set_listener:
  dds_delete_listener(dc->subinfo_listener);
err_create_subinfo_listener:
  dds_delete_listener(dc->quorum_listener);
err_create_quorum_listener:
  return 1;
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
   * increase the refcount for this participant as well. */
  ddsrt_atomic_inc32(&dc.refcount);
  dc.gv = gv;
  dc.cfg.quorum = DEFAULT_QOURUM;  /* LH: currently hardcoded set to 1, should be made configurable in future */
  dc.cfg.ident = ddsrt_strdup(DEFAULT_IDENT);
  ddsrt_avl_cinit(&server_td, &dc.servers);
  ddsrt_avl_cinit(&quorum_entry_td, &dc.quorum_entries);
  ddsrt_avl_cinit(&handle_to_quorum_entry_td, &dc.handle_to_quorum_entries);
  if ((dc.com = dc_com_new(&dc, domainid, gv))== NULL) {
    DDS_ERROR("failed to initialize the durable client infrastructure\n");
    goto err_com_new;
  }
  /* start a thread to process messages coming from a ds */
  ddsrt_threadattr_init(&dc.recv_tattr);
  if ((rc = ddsrt_thread_create(&dc.recv_tid, "dc", &dc.recv_tattr, recv_handler, &dc)) != DDS_RETCODE_OK) {
    goto err_recv_thread;
  }
  return DDS_RETCODE_OK;

err_recv_thread:
  dc_com_free(dc.com);
err_com_new:
  ddsrt_avl_cfree(&quorum_entry_td,  &dc.quorum_entries, cleanup_quorum_entry);
  ddsrt_avl_cfree(&server_td,  &dc.servers, cleanup_server);
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
    dc_com_free(dc.com);
    ddsrt_avl_cfree(&handle_to_quorum_entry_td,  &dc.handle_to_quorum_entries, cleanup_handle_to_quorum_entry);
    ddsrt_avl_cfree(&quorum_entry_td,  &dc.quorum_entries, cleanup_quorum_entry);
    ddsrt_avl_cfree(&server_td,  &dc.servers, cleanup_server);
  }
  ddsrt_free(dc.cfg.ident);
  return DDS_RETCODE_OK;
}

bool dds_durability_is_terminating (void)
{
  return (ddsrt_atomic_ld32(&dc.termflag) > 0);
}

void dds_durability_new_local_reader (struct dds_reader *reader, struct dds_rhc *rhc)
{
  /* create the administration to store transient data */
  /* create a durability reader that sucks and stores it in the store */

  (void)rhc;
  (void)reader;
  return;
}

/* This function checks if the writer has reached the quorum of matching durable containers
 *
 * It is NOT sufficient to verify if the quorum of durable services is reached.
 * If a writer would unblock when the quorum of durable services is reached, then
 * it is by no means certain that the durable writer has discovered the corresponding
 * data containers of these durable services. Data that has is published before the
 * data containers have been discovered by the writers, would not be delivered to
 * the data containers.
 *
 * For this reason, the quorum must be calculated based on the number of discovered
 * data containers for a writer. This also implies that if a writer has reached a quorum,
 * some other writer may not have reached the quorum yet.
 *
 * return
 *   DDS_RETCODE_OK             if quorum is reached
 *   DDS_PRECONDITION_NOT_MET   otherwise
 */
dds_return_t dds_durability_check_quorum_reached (struct dds_writer *writer)
{
  /* temporarily disabled */
  (void)writer;
  return DDS_RETCODE_OK;
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
    DDS_ERROR("failed to get qos from writer [%s]", dds_strretcode(rc));
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
    DDS_CLOG(DDS_LC_DUR, &dc.gv->logconfig, "durable writer \"%s\" subject to quorum checking\n", dc_stringify_id(wguid.v, id_str));
    if ((rc = dds_set_listener(writer, dc.quorum_listener)) < 0) {
      DDS_ERROR("Unable to set the quorum listener on writer \"%s\"\n", dc_stringify_id(wguid.v, id_str));
      goto err_set_listener;
    }
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

#if 0
dds_return_t dds_durability_wait_for_quorum (dds_writer *wr)
{
  dds_return_t ret = DDS_RETCODE_ERROR;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();
  ddsrt_mtime_t timeout;
  dds_duration_t sleep_duration;
  dds_entity_t writer;

  /* This function is called from dds_write() while the writer lock is held.
   * To make sure that we temporarily release the writer lock
   * while we wait until the quorum has been reached, we need access
   * to the entity handle in order to acquire the lock each time
   * we recheck.*/
  writer = (dds_entity_t)wr->m_entity.m_hdllink.hdl;
  /* No need to check for quorum for non-durable writers */
  if (wr->m_wr->xqos->durability.kind <= DDS_DURABILITY_TRANSIENT_LOCAL) {
    ret = DDS_RETCODE_OK;
    goto done;
  }
  timeout = ddsrt_mtime_add_duration (tnow, wr->m_wr->xqos->reliability.max_blocking_time);
  /* If the quorum is reached we'll immediately return DDS_RETCODE_OK.
   * Note that for volatile and transient-local writers the quorum
   * is but definition reached, so this function will always return with
   * DDS_RETCODE_OK in those case. */
  if (wr->quorum_reached) {
    ret = DDS_RETCODE_OK;
    goto done;
  }
  /* The quorum for a durable writer is not reached.
   * We will head bang until the quorum is reached.
   * To prevent starvation we use a max 10ms sleep in between.
   * If the quorum is reached within the max_blocking_time,
   * DDS_RETCODE_OK is returned, otherwise DDS_PRECONDITION_NOT_MET
   * is returned. */
  do {
    sleep_duration = DDS_MSECS(10);
    dds_writer_unlock(wr);
    dds_sleepfor (sleep_duration);
    if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK) {
      goto err_writer_lock;
    }
    tnow = ddsrt_time_monotonic ();
    if (tnow.v >= timeout.v) {
      ret = DDS_RETCODE_PRECONDITION_NOT_MET;
      break;
    }
    if (wr->quorum_reached) {
      ret = DDS_RETCODE_OK;
      break;
    }
  } while (true);
done:
  dds_writer_unlock(wr);
err_writer_lock:
  return ret;
}
#endif

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
  /* retrieve quorum reached */
  *quorum_reached = wr->quorum_reached;
  dds_writer_unlock(wr);
err_writer_lock:
  return ret;
}

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
   * on the first call to dds_durability_get_quorum_reached(). */
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

