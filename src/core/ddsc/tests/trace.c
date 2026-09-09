// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <string.h>
#include "CUnit/Theory.h"
#include "dds/dds.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/process.h"
#include "test_util.h"

static char trace_name[128];
static unsigned trace_serial;

static void setup (void)
{
  snprintf (trace_name, sizeof (trace_name), "ddsc-trace-%"PRIdPID"-%u.log", ddsrt_getpid (), trace_serial++);
  FILE *fp = fopen (trace_name, "w");
  CU_ASSERT_NEQ_FATAL (fp, NULL);
  CU_ASSERT_GEQ (fputs ("previous run\n", fp), 0);
  CU_ASSERT_EQ (fclose (fp), 0);
  dds_set_trace_sink (NULL, NULL);
}

static void teardown (void)
{
  remove (trace_name);
}

enum trace_flags { APPEND = 1, RAW = 2, FAIL = 4, DISABLED = 8 };

static dds_entity_t trace_domain (dds_domainid_t id, const char *name, unsigned flags)
{
  if (flags & RAW)
  {
    struct ddsi_config cfg;
    ddsi_config_init_default (&cfg);
    cfg.tracefile = (char *) name;
    cfg.tracemask = (flags & DISABLED) ? 0 : DDS_LC_USER1;
    cfg.tracingAppendToFile = (flags & APPEND) != 0;
#ifdef DDS_HAS_FAKEUDP
    if (test_config_inherits_fakeudp ())
      cfg.transport_selector = DDSI_TRANS_FAKEUDP;
#endif
    struct ddsi_config_network_interface_listelem intf = {
      .cfg = { .name = "nonexistent-trace-test-interface", .presence_required = 1 }
    };
    if (flags & FAIL)
      cfg.network_interfaces = &intf;
    return dds_create_domain_with_rawconfig (id, &cfg);
  }
  char *config;
  ddsrt_asprintf (&config,
    "<Tracing><Category>%s</Category><OutputFile>%s</OutputFile>"
    "<AppendToFile>%s</AppendToFile></Tracing>%s",
    (flags & DISABLED) ? "" : "user1", name, (flags & APPEND) ? "true" : "false",
    (flags & FAIL) ? "<General><Interfaces><NetworkInterface name=\"nonexistent-trace-test-interface\"/></Interfaces></General>" : "");
  dds_entity_t domain = test_create_domain_from_env (id, config);
  ddsrt_free (config);
  return domain;
}

static void trace_marker (dds_entity_t domain, const char *marker)
{
  const struct ddsi_domaingv *gv = get_domaingv (domain);
  DDS_CLOG (DDS_LC_USER1, &gv->logconfig, "marker:%s\n", marker);
}

static char *read_trace (void)
{
  FILE *fp = fopen (trace_name, "r");
  CU_ASSERT_NEQ_FATAL (fp, NULL);
  size_t size = 0, capacity = 4096;
  char *text = ddsrt_malloc (capacity);
  while (!feof (fp) && !ferror (fp))
  {
    if (size == capacity - 1)
      text = ddsrt_realloc (text, capacity *= 2);
    size += fread (text + size, 1, capacity - size - 1, fp);
  }
  text[size] = 0;
  CU_ASSERT_EQ (ferror (fp), 0);
  CU_ASSERT_EQ (fclose (fp), 0);
  return text;
}

CU_TheoryDataPoints (ddsc_trace, shared_file) = {
  CU_DataPoints (unsigned, 0, RAW, APPEND, RAW | APPEND)
};

CU_Theory ((unsigned flags), ddsc_trace, shared_file)
{
  setup ();
  dds_entity_t a = trace_domain (50, trace_name, flags);
  CU_ASSERT_GT_FATAL (a, 0);
  trace_marker (a, "first");

  char alias[sizeof (trace_name) + 2];
  snprintf (alias, sizeof (alias), "./%s", trace_name);
#if !DDSRT_HAVE_FILESYSTEM
  strcpy (alias, trace_name);
#endif
  dds_entity_t b = trace_domain (51, alias, (flags ^ RAW) & RAW);
  CU_ASSERT_GT_FATAL (b, 0);
  trace_marker (b, "second");
  CU_ASSERT_EQ (dds_delete (a), DDS_RETCODE_OK);
  trace_marker (b, "survivor");
  CU_ASSERT_EQ (dds_delete (b), DDS_RETCODE_OK);

  /* No domain remains: the next domain still belongs to the same trace run. */
  a = trace_domain (50, trace_name, flags & RAW);
  CU_ASSERT_GT_FATAL (a, 0);
  trace_marker (a, "recreated");
  CU_ASSERT_EQ (dds_delete (a), DDS_RETCODE_OK);

  char *text = read_trace ();
  CU_ASSERT_EQ (strstr (text, "previous run") != NULL, (flags & APPEND) != 0);
  CU_ASSERT_NEQ (strstr (text, "marker:first\n"), NULL);
  CU_ASSERT_NEQ (strstr (text, "marker:second\n"), NULL);
  CU_ASSERT_NEQ (strstr (text, "marker:survivor\n"), NULL);
  CU_ASSERT_NEQ (strstr (text, "marker:recreated\n"), NULL);
  ddsrt_free (text);
  teardown ();
}

CU_TheoryDataPoints (ddsc_trace, failed_domain) = {
  CU_DataPoints (unsigned, 0, RAW)
};

CU_Theory ((unsigned flags), ddsc_trace, failed_domain)
{
  setup ();
  /* Failure occurs after tracing is acquired, during interface selection. */
  CU_ASSERT_LT_FATAL (trace_domain (50, trace_name, flags | FAIL), 0);
  dds_entity_t a = trace_domain (50, trace_name, flags);
  CU_ASSERT_GT_FATAL (a, 0);
  char *failed_trace = read_trace ();
  CU_ASSERT_NEQ (strstr (failed_trace, "does not match an available interface"), NULL);
  ddsrt_free (failed_trace);
  trace_marker (a, "before-failure");
  CU_ASSERT_LT_FATAL (trace_domain (51, trace_name, (flags ^ RAW) | FAIL), 0);
  trace_marker (a, "after-failure");
  CU_ASSERT_EQ (dds_delete (a), DDS_RETCODE_OK);

  char *text = read_trace ();
  CU_ASSERT_EQ (strstr (text, "previous run"), NULL);
  CU_ASSERT_NEQ (strstr (text, "does not match an available interface"), NULL);
  CU_ASSERT_NEQ (strstr (text, "marker:before-failure\n"), NULL);
  CU_ASSERT_NEQ (strstr (text, "marker:after-failure\n"), NULL);
  ddsrt_free (text);
  teardown ();
}

CU_Test (ddsc_trace, disabled_and_standard_streams, .init = setup, .fini = teardown)
{
  dds_entity_t a = trace_domain (50, trace_name, RAW | DISABLED);
  CU_ASSERT_GT_FATAL (a, 0);
  CU_ASSERT_EQ (dds_delete (a), DDS_RETCODE_OK);
  char *text = read_trace ();
  CU_ASSERT_STREQ (text, "previous run\n");
  ddsrt_free (text);

  a = trace_domain (50, trace_name, RAW);
  CU_ASSERT_GT_FATAL (a, 0);
  trace_marker (a, "enabled");
  CU_ASSERT_EQ (dds_delete (a), DDS_RETCODE_OK);
  text = read_trace ();
  CU_ASSERT_EQ (strstr (text, "previous run"), NULL);
  CU_ASSERT_NEQ (strstr (text, "marker:enabled\n"), NULL);
  ddsrt_free (text);

  a = trace_domain (50, "stdout", RAW);
  CU_ASSERT_GT_FATAL (a, 0);
  CU_ASSERT_EQ (get_domaingv (a)->config.tracefp, stdout);
  CU_ASSERT_EQ (dds_delete (a), DDS_RETCODE_OK);
  a = trace_domain (50, "stderr", RAW);
  CU_ASSERT_GT_FATAL (a, 0);
  CU_ASSERT_EQ (get_domaingv (a)->config.tracefp, stderr);
  CU_ASSERT_EQ (dds_delete (a), DDS_RETCODE_OK);
  CU_ASSERT_EQ (fflush (stdout), 0);
  CU_ASSERT_EQ (fflush (stderr), 0);
}
