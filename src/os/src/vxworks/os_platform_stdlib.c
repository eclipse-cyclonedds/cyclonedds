/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <string.h>
#include <unistd.h>
#include <hostLib.h> /* MAXHOSTNAMELEN */

#include "os/os.h"

#ifdef _WRS_KERNEL
#include <envLib.h>

extern char *os_environ[];

void
os_stdlibInitialize(
    void)
{
   char **varset;

   for ( varset = &os_environ[0]; *varset != NULL; varset++ )
   {
      char *savePtr=NULL;
      char *varName;
      char *tmp = os_strdup( *varset );
      varName = strtok_r( tmp, "=", &savePtr );
      if ( os_getenv( varName ) == NULL )
      {
         os_putenv( *varset );
      }
      os_free(tmp);
   }
}
#endif

#define OS_HAS_STRTOK_R 1 /* FIXME: Should be handled by CMake */
#include "../snippets/code/os_gethostname.c"
#include "../snippets/code/os_stdlib.c"
#include "../snippets/code/os_stdlib_bsearch.c"
#include "../snippets/code/os_stdlib_strtod.c"
#include "../snippets/code/os_stdlib_strtol.c"
#include "../snippets/code/os_stdlib_strtok_r.c"
