// Copyright(c) 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "test_oneliner.h"

int main (int argc, char **argv)
{
  bool litmode = false;
  if (argc == 2 && strcmp (argv[1], "-l") == 0)
  {
    /* literate mode: test code lines start with "> ", everything
       else is just gobbledygook to be ignored */
    argc--;
    argv++;
    litmode = true;
  }
  if (argc > 1)
  {
    /* treat each argument as a test to execute */
    for (int i = 1; i < argc; i++)
    {
      if (test_oneliner (argv[i]) <= 0)
        return 1;
    }
  }
  else
  {
    /* read from stdin, # starts a comment, any line with an indent no greater
       than the current test's indent starts a new test, i.e.,
         # this is a comment
         r wr w 1
           take{(1,0,0)} r
         r disp w 1 # this is also a comment
           take{1} r
       contains two tests */
    struct oneliner_ctx ctx;
    size_t test_indent = SIZE_MAX;
    unsigned lineno = 0, test_begin = 0, test_end = 0;
    char buf[4096];
    while (fgets (buf, (int) sizeof (buf), stdin) != NULL)
    {
      lineno++;
      if (buf[strlen (buf) - 1] != '\n' && !feof (stdin))
      {
        if (test_indent < SIZE_MAX)
          (void) test_oneliner_fini (&ctx);
        fprintf (stderr, "stdin:%u: line too long\n", lineno);
        return 1;
      }

      if (litmode)
      {
        if (strncmp (buf, "> ", 2) != 0)
          continue;
        buf[0] = ' ';
      }

      char *cmt = strchr (buf, '#');
      if (cmt)
        *cmt = 0;

      size_t indent = 0, idx = 0;
      while (buf[idx] == ' ' || buf[idx] == '\t')
        indent += (buf[idx++] == ' ' ? 1 : 8 - (indent % 8));
      while (isspace ((unsigned char) buf[idx]))
        idx++;
      if (buf[idx] == 0)
        continue;

      if (indent <= test_indent)
      {
        if (test_indent < SIZE_MAX)
        {
          if (test_oneliner_fini (&ctx) <= 0)
          {
            fprintf (stderr, "stdin:%u-%u: FAIL: %s\n", test_begin, test_end, test_oneliner_message (&ctx));
            return 1;
          }
          printf ("\n");
        }
        printf ("------ stdin:%u ------\n", lineno);
        test_indent = indent;
        test_begin = lineno;
        test_oneliner_init (&ctx, NULL);
      }

      test_oneliner_step (&ctx, buf + idx);
      test_end = lineno;
    }

    if (test_indent < SIZE_MAX && test_oneliner_fini (&ctx) <= 0)
      fprintf (stderr, "stdin:%u-%u: FAIL: %s\n", test_begin, test_end, test_oneliner_message (&ctx));
    if (ferror (stdin))
    {
      fprintf (stderr, "error reading stdin\n");
      return 1;
    }
  }
  return 0;
}
