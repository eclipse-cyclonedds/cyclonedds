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

#define TRACE(...) DDS_CLOG (DDS_LC_DUR, &domaingv->logconfig, __VA_ARGS__)

struct known_ds_t {
  ddsrt_avl_node_t node;
  DurableSupport_id_t id; /* id key */
  char id_str[37]; /* cached string representation of the id */
  char *hostname;  /* advertised hostname */
  char *name;  /* human readable name */
};

static char *dc_stringify_id(const DurableSupport_id_t id, char *buf)
{
  assert(buf);
  snprintf (buf, 37, "%02x%02x%02x%02x\055%02x%02x\055%02x%02x\055%02x%02x\055%02x%02x%02x%02x%02x%02x",
                id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7],
                id[8], id[9], id[10], id[11], id[12], id[13], id[14], id[15]);
  return buf;
}

static int cmp_known_ds (const void *a, const void *b)
{
  return memcmp(a, b, 16);
}

static void cleanup_known_ds (void *n)
{
  struct known_ds_t *known_ds = (struct known_ds_t *)n;
  ddsrt_free(known_ds->hostname);
  ddsrt_free(known_ds->name);
  ddsrt_free(known_ds);
}

static const ddsrt_avl_ctreedef_t known_ds_td = DDSRT_AVL_CTREEDEF_INITIALIZER(offsetof (struct known_ds_t, node), offsetof (struct known_ds_t, id), cmp_known_ds, 0);

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
  ddsrt_avl_ctree_t known_ds; /* tree containing all known ds's */
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

static void evaluate_quorum_reached (struct dc_t *dc, const uint32_t cnt)
{
  if (dc->quorum_reached) {
    if (cnt < dc->cfg.quorum) {
      /* quorum changed from reached to not reached */
      dc->quorum_reached = false;
      DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "quorum droppped below threshold (%" PRIu32 ")\n", dc->cfg.quorum);
    }
  } else {
    if (cnt >= dc->cfg.quorum) {
      /* quorum changed from not reached to reached */
      dc->quorum_reached = true;
      DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "quorum threshold (%" PRIu32 ") reached\n", dc->cfg.quorum);
    }
  }
}

static struct known_ds_t *create_known_ds (struct dc_t *dc, DurableSupport_id_t id, const char *name, const char *hostname)
{
  struct known_ds_t *known_ds;
  char id_str[37]; /* guid */

  assert(name);
  assert(hostname);
  /* the ds is not known yet by the dc, let's create it */
  if ((known_ds = (struct known_ds_t *)ddsrt_malloc(sizeof(struct known_ds_t))) == NULL) {
    goto err_alloc_known_ds;
  }
  memcpy(known_ds->id, id, 16);
  dc_stringify_id(known_ds->id, known_ds->id_str);
  known_ds->name = ddsrt_strdup(name);
  known_ds->hostname = ddsrt_strdup(hostname);
  ddsrt_avl_cinsert(&known_ds_td, &dc->known_ds, known_ds);
  return known_ds;

err_alloc_known_ds:
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
  /* todo: perhaps add a configurable set of request partitions */
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

static void dc_lost_ds (struct dc_t *dc, DurableSupport_status *status)
{
  struct known_ds_t *known_ds;
  uint32_t total;

  /* lookup the ds entry in the list of available ds's */
  if ((known_ds = ddsrt_avl_clookup (&known_ds_td, &dc->known_ds, status->id)) == NULL)  {
    /* ds not known, so nothing lost */
    return;
  }
  ddsrt_avl_cdelete(&known_ds_td, &dc->known_ds, known_ds);
  total = (uint32_t)ddsrt_avl_ccount(&dc->known_ds);
  DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "durable service \"%s\" (%s@%s) lost (total: %" PRIu32 ")\n", known_ds->id_str, known_ds->name, known_ds->hostname, total);
  cleanup_known_ds(known_ds);
  /* a ds has been removed.
   * we might have to reevaluate the quorum */
  evaluate_quorum_reached(dc, total);
}

static void dc_ds_discovered (struct dc_t *dc, DurableSupport_status *status)
{
  struct known_ds_t *known_ds;
  uint32_t total;

  /* lookup the ds entry in the list of available ds's */
  if ((known_ds = ddsrt_avl_clookup (&known_ds_td, &dc->known_ds, status->id)) != NULL)  {
    /* ds already known */
    return;
  }
  known_ds = create_known_ds(dc, status->id, status->name, status->hostname);
  total = (uint32_t)ddsrt_avl_ccount(&dc->known_ds);
  DDS_CLOG(DDS_LC_DUR, &dc->gv->logconfig, "durable service \"%s\" (%s@%s) discovered (total: %" PRIu32 ")\n", known_ds->id_str, known_ds->name, known_ds->hostname, total);
  /* reevalute if the quorum has been reached */
  evaluate_quorum_reached(dc, total);
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
        dc_lost_ds(dc, status);
      } else if (info[j].valid_data) {
        /* a DS is available */

        /* todo: check if the participant of the status topic contains the IDENT,
         * so we are sure that this is a durable . This prevents that the client
         * is fooled that a ds is present when some malicious node sends a
         * status without the IDENT. However, for testing purposes we actually
         * might want to fool a client.
         *
         * We might want to use dds_get_matched_publication_data() to retrieve the
         * builtin endpoint for the writer that wrote the data, and check it's
         * participant for the presence of the IDENT. */

        dc_ds_discovered(dc, status);
      }
    }
  }
  return samplecount;
#undef MAX_SAMPLES
}


static uint32_t recv_handler (void *a)
{
  struct dc_t *dc = (struct dc_t *)a;
  dds_duration_t timeout = DDS_INFINITY;
  dds_attach_t wsresults[1];
  size_t wsresultsize = sizeof(wsresults)/sizeof(wsresults[0]);
  int n;
  int j;

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
  /* LH: the quorum is now initialized to 0.
   * if set to 1, the transient_publisher cannot terminate if it has writers that are blocked.
   * This is because the writers are only unblocked when dds_durability_fini(),
   * but this function never gets called because the blocking transient_publisher application
   * keeps a participant alive. */
  dc.cfg.quorum = 1;
   ddsrt_avl_cinit(&known_ds_td, &dc.known_ds);
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
  dc.com = NULL;
err_com_new:
  ddsrt_avl_cfree(&known_ds_td,  &dc.known_ds, cleanup_known_ds);
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
    ddsrt_avl_cfree(&known_ds_td,  &dc.known_ds, cleanup_known_ds);
  }
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
#if 0
  uint32_t nds;


  if (writer == NULL) {
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  }
  /* quorum not reached if the number of discovered ds's is less than the quorum */
  if ((nds = (uint32_t)ddsrt_avl_ccount(&dc.known_ds)) < dc.cfg.quorum) {
    printf("LH *** not enough ds\n");
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  }
  /* */
#endif

  return DDS_RETCODE_OK;
}
