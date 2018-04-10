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
#include "os/os.h"
#include <stdlib.h>
#include <assert.h>

/** \brief Register an process exit handler
 *
 * \b os_procAtExit registers an process exit
 * handler by calling \b atexit passing the \b function
 * to be called when the process exits.
 * The standard POSIX implementation guarantees the
 * required order of execution of the exit handlers.
 */
os_result
os_procAtExit(
    _In_ void (*function)(void))
{
    int result;
    os_result osResult;

    assert (function != NULL);

    result = atexit (function);
    if(!result)
    {
        osResult = os_resultSuccess;
    } else
    {
        osResult = os_resultFail;
    }
    return osResult;
}
