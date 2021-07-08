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
#include <assert.h>
#include <stdio.h>

#include "dds/dds.h"
#include "dds/ddsrt/log.h"
#include "CUnit/Test.h"

#define PEERS(...) "<CycloneDDS><Domain><Discovery><Peers>" __VA_ARGS__ "</Peers></Discovery></Domain></CycloneDDS>"
#define TRACING(...) "<CycloneDDS><Domain><Tracing><Category>" __VA_ARGS__ "</Category></Tracing></Domain></CycloneDDS>"
#define PEER(host) "<Peer address=\"" #host "\"></Peer>"
#define TRACE(category) #category

CU_Test(ddsc_reload, peers)
{
  const char *config;
  dds_entity_t domain;
  dds_return_t ret;

  //dds_set_log_mask(DDS_LC_ALL);
  //config = PEERS( PEER(1.1.1.1) );
  config = TRACING( TRACE(trace) );
  // config = PEERS( ); // Uncomment to verify rf_peer is not called the second time
  fprintf(stderr, "(1) config: %s\n", config);
  domain = dds_create_domain(1, config);
  CU_ASSERT_FATAL(domain > 0);
  config = PEERS( PEER(2.2.2.2) PEER(3.3.3.3) );
  config = TRACING( TRACE(discovery) );
  fprintf(stderr, "(2) config: %s\n", config);
  ret = dds_reload_domain_config(domain, config);
  fprintf(stderr, "(2) result: %s\n", (ret == DDS_RETCODE_OK) ? "true" : "false");
  config = TRACING( );
  fprintf(stderr, "(3) config: %s\n", config);
  ret = dds_reload_domain_config(domain, config);
  fprintf(stderr, "(3) result: %s\n", (ret == DDS_RETCODE_OK) ? "true" : "false");
}
