/*
 * Copyright(c) 2026 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#define _CRT_SECURE_NO_WARNINGS // sscanf

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "dds/dds.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"

#include "dyntypelib.h"

#include "dds/ddsi/ddsi_serdata.h"

#if ENABLE_RAW_SAMPLE_BYPASS
#include "dds__serdata_default.h"
#endif

ddsrt_nonnull_all
ddsrt_attribute_noreturn
ddsrt_attribute_format_printf (1, 2)
static void exitfmt (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  exit (1);
}

static bool lookup_type_pair (struct dyntypelib *dtl, const char *names, struct dyntype **wrtype, struct dyntype **rdtype)
{
  char *wtname = strdup (names);
  if (wtname == NULL)
    return false;
  char *rtname = strchr (wtname, '/');
  bool res = true;
  if (rtname)
    *rtname++ = 0;
  else
    rtname = wtname;
  *wrtype = *rdtype = NULL;
  if (*wtname) {
    printf ("T wr %s = ", wtname); fflush (stdout);
    if ((*wrtype = dtl_lookup_typename (dtl, wtname)) == NULL) {
      printf ("unknown\n");
      res = false;
    } else {
      printf ("%s\n", (*wrtype)->name);
    }
  }
  if (*rtname) {
    printf ("T rd %s = ", rtname); fflush (stdout);
    if ((*rdtype = dtl_lookup_typename (dtl, rtname)) == NULL) {
      printf ("unknown\n");
      res = false;
    } else {
      printf ("%s\n", (*rdtype)->name);
    }
  }
  free (wtname);
  return res;
}

static bool doread (struct dyntypelib *dtl, const dds_entity_t ws, const dds_entity_t rd, DDS_XTypes_TypeObject const * const typeobj, bool exit_on_timeout)
{
  dds_return_t rc;
  rc = dds_waitset_wait (ws, NULL, 0, DDS_SECS (1));
  if (rc < 0)
    exitfmt ("dds_waitset_wait: %s\n", dds_strretcode (rc));
  if (rc == 0 && exit_on_timeout)
    return false;
  void *ptr = NULL;
  dds_sample_info_t si;
  while ((rc = dds_take (rd, &ptr, &si, 1, 1)) == 1)
  {
    dtl_print_sample (dtl, si.valid_data, ptr, &typeobj->_u.complete);
    dds_return_loan (rd, &ptr, 1);
  }
  if (rc < 0)
    exitfmt ("dds_take: %s\n", dds_strretcode (rc));

  dds_requested_incompatible_qos_status_t riq;
  rc = dds_get_requested_incompatible_qos_status (rd, &riq);
  if (rc < 0)
    exitfmt ("dds_get_requested_incompatible_qos_status: %s\n", dds_strretcode (rc));
  if (riq.total_count_change != 0)
    printf ("riq policy %"PRIu32"\n", riq.last_policy_id);

  dds_subscription_matched_status_t sm;
  rc = dds_get_subscription_matched_status (rd, &sm);
  if (rc < 0)
    exitfmt ("dds_get_subscription_matched_status: %s\n", dds_strretcode (rc));
  return (riq.total_count == 0 && (sm.current_count > 0 || sm.current_count_change >= 0));
}

static void usage (const char *argv0)
{
  fprintf (stderr, "usage: %s [OPTIONS] xmlfile TYPE DATA...\n\
\n\
OPTIONS:\n\
-c BINDIGITS   set type consistency enforcement to BINDIGITS, which must\n\
               consist of five 0 or 1 digits:\n\
                - ignore sequence bounds\n\
                - ignore string bounds\n\
                - ignore member names\n\
                - prevent type widening\n\
                - force type validation\n\
",
           argv0);
  exit (2);
}

int main (int argc, char **argv)
{
  dds_return_t rc;
  int opt;
  uint32_t tce = 0x18; // default true,true,false,false,false
  bool skip_normalize_for_bin = false;
  while ((opt = getopt (argc, argv, "c:R")) != EOF)
  {
    switch (opt)
    {
      case 'c':
        if (strspn (optarg, "01") != 5 || optarg[5] != 0) {
          fprintf (stderr, "%s: %s is not a valid type consistency enforcement setting\n", argv[0], optarg);
          exit (2);
        }
        tce = 0;
        for (const char *p = optarg; *p; p++)
          tce = (tce << 1) | (*p == '1');
        break;
      case 'R':
        skip_normalize_for_bin = true;
        break;
      default:
        usage (argv[0]);
        break;
    }
  }

  if (argc - optind < 2)
    usage (argv[0]);

  dds_entity_t dp = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (dp < 0)
    exitfmt ("%s: create_participant failed: %s\n", argv[0], dds_strretcode (dp));

  struct dyntypelib *dtl = dtl_new (dp);
  struct dyntypelib_error err;

  // Load type libraries
  int argi = optind;
  while (argi < argc && strlen (argv[argi]) > 4 && strcmp (argv[argi] + strlen (argv[argi]) - 4, ".xml") == 0)
  {
    if (dtl_add_xml_type_library (dtl, argv[optind], &err) != DDS_RETCODE_OK)
      exitfmt ("%s: %s: %s\n", argv[0], argv[optind], err.errmsg);
    argi++;
  }

  struct dyntype *wrtype = NULL, *rdtype = NULL;
  dds_topic_descriptor_t *wrdescriptor = NULL, *rddescriptor = NULL;
  dds_entity_t wrtp = 0, rdtp = 0, wr = 0, rd = 0, ws = 0;
  for (; argi < argc; argi++)
  {
    size_t arglen = strlen (argv[argi]);
    if (arglen <= 4 || (strcmp (argv[argi] + arglen - 4, ".xml") != 0 && strcmp (argv[argi] + arglen - 4, ".bin") != 0))
    {
      if (!lookup_type_pair (dtl, argv[argi], &wrtype, &rdtype))
        exitfmt ("\ncreate topic: type lookup failed\n");

      // Can be freed immediately after creating topic, but we use it for freeing samples
      if (wrdescriptor)
        dds_delete_topic_descriptor (wrdescriptor);
      if (rddescriptor)
        dds_delete_topic_descriptor (rddescriptor);

      dds_delete (ws); ws = 0;
      dds_delete (rd); rd = 0;
      dds_delete (wr); wr = 0;
      dds_delete (wrtp); wrtp = 0;
      dds_delete (rdtp); rdtp = 0;

      dds_qos_t *tpqos = dds_create_qos ();
      dds_qset_reliability (tpqos, DDS_RELIABILITY_RELIABLE, DDS_SECS (1));
      dds_qos_t *epqos = dds_create_qos ();
      dds_qset_type_consistency (
              epqos, DDS_TYPE_CONSISTENCY_ALLOW_TYPE_COERCION,
              (tce >> 4) & 1,
              (tce >> 3) & 1,
              (tce >> 2) & 1,
              (tce >> 1) & 1,
              tce & 1);

      if (wrtype) {
        rc = dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, dp, wrtype->typeinfo, 0, &wrdescriptor);
        if (rc != 0)
          exitfmt ("dds_create_topic_descriptor: %s\n", dds_strretcode (rc));
        wrtp = dds_create_topic (dp, wrdescriptor, "T", tpqos, NULL);
        if (wrtp < 0)
          exitfmt ("dds_create_topic: %s\n", dds_strretcode (wrtp));
        wr = dds_create_writer (dp, wrtp, epqos, NULL);
        if (wr < 0)
          exitfmt ("dds_create_writer: %s\n", dds_strretcode (wr));
        rc = dds_set_status_mask (wr, DDS_OFFERED_INCOMPATIBLE_QOS_STATUS);
        if (rc != 0)
          exitfmt ("dds_set_status_mask: %s\n", dds_strretcode (rc));
      }
      if (rdtype) {
        rc = dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, dp, rdtype->typeinfo, 0, &rddescriptor);
        if (rc != 0)
          exitfmt ("dds_create_topic_descriptor: %s\n", dds_strretcode (rc));
        rdtp = dds_create_topic (dp, rddescriptor, "T", tpqos, NULL);
        if (rdtp < 0)
          exitfmt ("dds_create_topic: %s\n", dds_strretcode (rdtp));
        rd = dds_create_reader (dp, rdtp, epqos, NULL);
        if (rd < 0)
          exitfmt ("dds_create_reader: %s\n", dds_strretcode (rd));
        rc = dds_set_status_mask (rd, DDS_DATA_AVAILABLE_STATUS | DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS);
        if (rc != 0)
          exitfmt ("dds_set_status_mask: %s\n", dds_strretcode (rc));
        ws = dds_create_waitset (dp);
        if (ws < 0)
          exitfmt ("dds_create_waitset: %s\n", dds_strretcode (rd));
        rc = dds_waitset_attach (ws, rd, 0);
        if (rc != 0)
          exitfmt ("dds_waitset_attach reader: %s\n", dds_strretcode (rc));
      }

      dds_delete_qos (epqos);
      dds_delete_qos (tpqos);

      size_t align, size;
      if (wrtype)
      {
        build_typecache_to (dtl->typecache, &wrtype->typeobj->_u.complete, &align, &size);
        ppc_print_to (dtl->typecache, &dtl->ppc, &wrtype->typeobj->_u.complete);
      }
      if (rdtype)
      {
        build_typecache_to (dtl->typecache, &rdtype->typeobj->_u.complete, &align, &size);
        ppc_print_to (dtl->typecache, &dtl->ppc, &rdtype->typeobj->_u.complete);
      }

      // short sleep before writing so a remote reader is likely to have been discovered before the sample is written
      dds_sleepfor (DDS_MSECS (100));
    }
    else if (strcmp (argv[argi] + arglen - 4, ".xml") == 0)
    {
      // data file
      if (wr == 0)
        exitfmt ("%s: data file given, but no writer type set yet\n", argv[argi]);
      assert (wrtype);

      struct elem *input = domtree_from_file (argv[argi]);
      if (input == NULL)
        exitfmt ("%s: %s: can't read sample\n", argv[0], argv[argi]);
      domtree_print (input);
      void *sample = dtl_scan_sample (dtl, input, &wrtype->typeobj->_u.complete, true, &err);
      if (sample == NULL)
        exitfmt ("%s: %s: can't convert to sample: %s\n", argv[0], argv[argi], err.errmsg);
      if ((rc = dds_write (wr, sample)) != 0)
        exitfmt ("%s: %s: can't write: %s\n", argv[0], argv[argi], dds_strretcode (rc));
      const struct dds_cdrstream_allocator a = {
        .malloc = ddsrt_malloc,
        .free = ddsrt_free,
        .realloc = ddsrt_realloc };
      dds_stream_free_sample (sample, &a, wrdescriptor->m_ops);
      ddsrt_free (sample);

      if (rd)
        doread (dtl, ws, rd, rdtype->typeobj, true);

      // sleep a bit after writing data
      if (argi < argc)
        dds_sleepfor (DDS_MSECS (500));
    }
    else if (strcmp (argv[argi] + arglen - 4, ".bin") == 0)
    {
      // ONE BIG HACK
      if (wr == 0)
        exitfmt ("%s: data file given, but no writer type set yet\n", argv[argi]);
      assert (wrtype);

      FILE *fp = fopen (argv[argi], "rb");
      if (fp == NULL)
        exitfmt ("%s: %s: can't open file\n", argv[0], argv[argi]);
      size_t bufcap = 1000;
      size_t bufsz = 0;
      char *buf = ddsrt_malloc (bufcap);
      while (!feof(fp) && !ferror(fp))
      {
        if (bufsz == bufcap)
          buf = ddsrt_realloc (buf, bufcap *= 2);
        bufsz += fread (buf + bufsz, 1, bufcap - bufsz, fp);
      }
      if (ferror (fp))
        exitfmt ("%s: %s: error reading file\n", argv[0], argv[argi]);
      if (bufsz < 4)
        exitfmt ("%s: %s: not enough space for an encoding header\n", argv[0], argv[argi]);
      fclose (fp);

      struct ddsi_serdata *sd;
      const struct ddsi_sertype *st;
      dds_get_entity_sertype (wr, &st);

      if (skip_normalize_for_bin)
      {
#if ENABLE_RAW_SAMPLE_BYPASS
        void *sample = calloc (1, st->sizeof_type);
        sd = ddsi_serdata_from_sample (st, SDK_DATA, sample);
        ddsrt_free (sample);
        if (sd == NULL || sd == DDSI_SERDATA_FROM_SER_DISCARD)
          exitfmt ("%s: %s: failed to convert all-zero sample to serdata\n", argv[0], argv[argi]);
        struct dds_serdata_default *sdd;
        sdd = ddsrt_realloc (sd, offsetof (struct dds_serdata_default, hdr) + ((bufsz + 3) & ~(size_t)0x3));
        sdd->pos = sdd->size = (uint32_t) bufsz - 4;
        memcpy (&sdd->hdr, buf, bufsz);
        sd = (struct ddsi_serdata *) sdd;
#else
        exitfmt ("%s: injecting raw data while bypassing normalization in this build\n", argv[0]);
#endif
      }
      else
      {
        ddsrt_iovec_t iov = { .iov_len = (ddsrt_iov_len_t) bufsz, .iov_base = buf };
        sd = ddsi_serdata_from_ser_iov (st, SDK_DATA, 1, &iov, bufsz);
        if (sd == NULL || sd == DDSI_SERDATA_FROM_SER_DISCARD)
          exitfmt ("%s: %s: failed to convert CDR to serdata\n", argv[0], argv[argi]);
      }

      sd->timestamp = ddsrt_time_wallclock ();
      sd->statusinfo = 0;

      if ((rc = dds_forwardcdr (wr, sd)) != 0)
        exitfmt ("%s: %s: can't forward: %s\n", argv[0], argv[argi], dds_strretcode (rc));

      if (rd)
        doread (dtl, ws, rd, rdtype->typeobj, true);

      // sleep a bit after writing data
      if (argi < argc)
        dds_sleepfor (DDS_MSECS (500));
    }
  }

  if (wr)
  {
    dds_offered_incompatible_qos_status_t oiq;
    rc = dds_get_offered_incompatible_qos_status (wr, &oiq);
    if (rc < 0)
      exitfmt ("dds_get_offered_incompatible_matched_status: %s\n", dds_strretcode (rc));
    if (oiq.total_count_change != 0)
      printf ("oiq policy %"PRIu32"\n", oiq.last_policy_id);
  }

  if (wr)
    dds_delete (wr);
  if (rd)
  {
    while (doread (dtl, ws, rd, rdtype->typeobj, false))
      ;
  }

  dtl_free (dtl);
  dds_delete (dp);
  return 0;
}
