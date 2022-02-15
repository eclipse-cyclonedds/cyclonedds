/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
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
#include <stdint.h>
#include <string.h>
#include <dds/dds.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/q_addrset.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_builtin_topic_if.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/ddsi_rhc.h"
#include "dds/ddsi/ddsi_vnet.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_bswap.h"
#include "dds__whc.h"
#include "dds__types.h"

static struct cfgst *cfgst;
static struct ddsi_domaingv gv;
static struct ddsi_config cfg;
static ddsi_tran_conn_t fakeconn;
static ddsi_tran_factory_t fakenet;
static struct thread_state1 *ts1;
static struct nn_rbufpool *rbpool;
// static struct ddsi_tkmap_instance *tk;

static void null_log_sink(void *varg, const dds_log_data_t *msg)
{
    (void)varg;
    (void)msg;
}

static ssize_t fakeconn_write(ddsi_tran_conn_t conn, const ddsi_locator_t *dst, size_t niov, const ddsrt_iovec_t *iov, uint32_t flags)
{
    return (ssize_t)niov;
}

static ssize_t fakeconn_read(ddsi_tran_conn_t conn, unsigned char *buf, size_t len, bool allow_spurious, ddsi_locator_t *srcloc)
{
    return (ssize_t)len;
}

int LLVMFuzzerTestOneInput(
    const uint8_t *data,
    size_t size)
{
    if (!size)
        return EXIT_FAILURE;

    ddsi_iid_init();
    thread_states_init(64);

    memset(&dds_global, 0, sizeof(dds_global));
    ddsrt_mutex_init(&dds_global.m_mutex);

    ddsi_config_init_default(&gv.config);

    memset(&gv, 0, sizeof(gv));

    char *str = NULL;

    if ((str = (char *)malloc(size + 1)) == NULL)
        return EXIT_FAILURE;

    memcpy(str, data, size);
    str[size] = '\0';

    if ((cfgst = ddsi_config_init(str, &gv.config, 0)) == NULL)
    {
        free(str);
        return EXIT_FAILURE;
    }

    free(str);
    ddsi_config_fini(cfgst);
    return EXIT_SUCCESS;
}
