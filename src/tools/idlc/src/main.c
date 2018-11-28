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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "parser.h"

static void
usage(const char *prog)
{
  fprintf(stderr, "Usage: %s FILE\n", prog);
}

static void report_error(int line, int column, const char *msg)
{
  fprintf(stderr, "ERROR %d.%d: %s\n", line, column, msg);
}

int
main(int argc, char *argv[])
{
  int ret = EXIT_FAILURE;

  if (argc != 2) {
    usage(argv[0]);
  } else if (dds_ts_parse_file(argv[1], report_error) == 0) {
    ret = EXIT_SUCCESS;
  }

  return ret;
}
