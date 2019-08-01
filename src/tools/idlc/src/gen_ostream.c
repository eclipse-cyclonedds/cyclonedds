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
#include <stdio.h>
#include <stdbool.h>
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/misc.h"

#include "gen_ostream.h"


extern bool ddsts_ostream_open(ddsts_ostream_t *ostream, const char *name)
{
  return ostream->open(ostream, name);
}

extern void ddsts_ostream_close(ddsts_ostream_t *ostream)
{
  ostream->close(ostream);
}

extern void ddsts_ostream_put(ddsts_ostream_t *ostream, char ch)
{
  ostream->put(ostream, ch);
}

extern void ddsts_ostream_puts(ddsts_ostream_t *ostream, const char *str)
{
  for (; *str != '\0'; str++) {
    ostream->put(ostream, *str);
  }
}

/* output stream to null */

typedef struct {
  ddsts_ostream_t ostream;
} ostream_to_null_t;

static bool null_ostream_open(ddsts_ostream_t *ostream, const char *name)
{
  DDSRT_UNUSED_ARG(ostream);
  DDSRT_UNUSED_ARG(name);
  return true;
}

static void null_ostream_close(ddsts_ostream_t *ostream)
{
  DDSRT_UNUSED_ARG(ostream);
}

static void null_ostream_put(ddsts_ostream_t *ostream, char ch)
{
  DDSRT_UNUSED_ARG(ostream);
  DDSRT_UNUSED_ARG(ch);
}

extern dds_return_t ddsts_create_ostream_to_null(ddsts_ostream_t **ref_ostream)
{
  ostream_to_null_t *ostream_to_null = (ostream_to_null_t*)ddsrt_malloc(sizeof(ostream_to_null_t));
  if (ostream_to_null == NULL) {
    *ref_ostream = NULL;
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  ostream_to_null->ostream.open = null_ostream_open;
  ostream_to_null->ostream.close = null_ostream_close;
  ostream_to_null->ostream.put = null_ostream_put;
  *ref_ostream = &ostream_to_null->ostream;
  return DDS_RETCODE_OK;
}


/* output stream to files */

typedef struct {
  ddsts_ostream_t ostream;
  FILE *f;
} ostream_to_files_t;

static bool files_ostream_open(ddsts_ostream_t *ostream, const char *name)
{
DDSRT_WARNING_MSVC_OFF(4996);
  return (((ostream_to_files_t*)ostream)->f = fopen(name, "wt")) != 0;
DDSRT_WARNING_MSVC_ON(4996);
}

static void files_ostream_close(ddsts_ostream_t *ostream)
{
  fclose(((ostream_to_files_t*)ostream)->f);
}

static void files_ostream_put(ddsts_ostream_t *ostream, char ch)
{
  fputc(ch, ((ostream_to_files_t*)ostream)->f);
}

extern dds_return_t ddsts_create_ostream_to_files(ddsts_ostream_t **ref_ostream)
{
  ostream_to_files_t *ostream_to_files = (ostream_to_files_t*)ddsrt_malloc(sizeof(ostream_to_files_t));
  if (ostream_to_files == NULL) {
    *ref_ostream = NULL;
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  ostream_to_files->ostream.open = files_ostream_open;
  ostream_to_files->ostream.close = files_ostream_close;
  ostream_to_files->ostream.put = files_ostream_put;
  ostream_to_files->f = NULL;
  *ref_ostream = &ostream_to_files->ostream;
  return DDS_RETCODE_OK;
}

/* output stream to buffer */

typedef struct {
  ddsts_ostream_t ostream;
  char *s;
  const char *e;
} ostream_to_buffer_t;

static void buffer_ostream_put(ddsts_ostream_t *ostream, char ch)
{
  if (((ostream_to_buffer_t*)ostream)->s < ((ostream_to_buffer_t*)ostream)->e) {
    *((ostream_to_buffer_t*)ostream)->s++ = ch;
    *((ostream_to_buffer_t*)ostream)->s = '\0';
  }
}

static bool buffer_ostream_open(ddsts_ostream_t *ostream, const char* name)
{
  DDSRT_UNUSED_ARG(ostream);
  DDSRT_UNUSED_ARG(name);
  return true;
}

static void buffer_ostream_close(ddsts_ostream_t *ostream)
{
  DDSRT_UNUSED_ARG(ostream);
}

extern dds_return_t ddsts_create_ostream_to_buffer(char *buffer, size_t len, ddsts_ostream_t **ref_ostream)
{
  ostream_to_buffer_t *ostream_to_buffer = (ostream_to_buffer_t*)ddsrt_malloc(sizeof(ostream_to_buffer_t));
  if (ostream_to_buffer == NULL) {
    *ref_ostream = NULL;
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  ostream_to_buffer->ostream.open = buffer_ostream_open;
  ostream_to_buffer->ostream.close = buffer_ostream_close;
  ostream_to_buffer->ostream.put = buffer_ostream_put;
  ostream_to_buffer->s = buffer;
  ostream_to_buffer->e = buffer + len - 1;
  *ref_ostream = &ostream_to_buffer->ostream;
  return DDS_RETCODE_OK;
}

