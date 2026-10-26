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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dds/dds.h"
#include "dds/ddsc/dds_public_qos_provider.h"
#include "dds/ddsrt/log.h"

/* Fuzzes the system-definition (sysdef) XML parser through the public QoS
   provider API.  This reaches dds_sysdef_init_sysdef_str() and the whole of
   src/core/ddsc/src/dds_sysdef_parser.c, including the base64 and hex decoders
   used for the user_data/topic_data/group_data QoS policies.

   dds_create_qos_provider() interprets an argument starting with '<' as an
   inline sysdef document and anything else as a filesystem path.  Only the
   inline form is fuzzed: sending arbitrary bytes down the fopen() branch would
   make the target depend on the filesystem and stop being reproducible. */

#define MAX_INPUT_SZ 65536u

int LLVMFuzzerInitialize (int *argc, char ***argv)
{
  (void) argc;
  (void) argv;
  /* Malformed input makes the parser log on nearly every execution.  Writing
     that to stderr dominates the runtime, so silence everything the log mask
     allows to be silenced. */
  dds_set_log_mask (0);
  return 0;
}

int LLVMFuzzerTestOneInput (const uint8_t *data, size_t size)
{
  if (size == 0 || size > MAX_INPUT_SZ || data[0] != '<')
    return 0;

  char *xml = malloc (size + 1);
  if (xml == NULL)
    return 0;
  memcpy (xml, data, size);
  xml[size] = '\0';

  dds_qos_provider_t *provider = NULL;
  if (dds_create_qos_provider (xml, &provider) == DDS_RETCODE_OK)
  {
    /* Also exercise the keyed lookup path.  Keys are matched with strcmp against
       "<library>::<profile>", so they have to be literal names that appear in the
       seed corpus -- a wildcard would never resolve.  The provider owns the
       returned qos, so it must not be freed here. */
    static const char *const keys[] = { "L::P", "OurLibrary::ProfileA" };
    static const dds_qos_kind_t kinds[] = {
      DDS_PARTICIPANT_QOS, DDS_TOPIC_QOS, DDS_PUBLISHER_QOS,
      DDS_SUBSCRIBER_QOS, DDS_READER_QOS, DDS_WRITER_QOS
    };
    for (size_t k = 0; k < sizeof (keys) / sizeof (keys[0]); k++)
    {
      for (size_t i = 0; i < sizeof (kinds) / sizeof (kinds[0]); i++)
      {
        const dds_qos_t *qos = NULL;
        (void) dds_qos_provider_get_qos (provider, kinds[i], keys[k], &qos);
      }
    }
    dds_delete_qos_provider (provider);
  }

  free (xml);
  return 0;
}
