/*
 * Copyright(c) 2021 Apex.AI Inc. All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include "dds/ddsi/shm_init.h"
#include "iceoryx_binding_c/log.h"

static enum iox_LogLevel to_iox_loglevel(enum ddsi_shm_loglevel level) {
    switch(level) {
        case DDSI_SHM_OFF : return Iceoryx_LogLevel_Off;
        case DDSI_SHM_FATAL : return Iceoryx_LogLevel_Fatal;
        case DDSI_SHM_ERROR : return Iceoryx_LogLevel_Error;
        case DDSI_SHM_WARN : return Iceoryx_LogLevel_Warn;
        case DDSI_SHM_INFO : return Iceoryx_LogLevel_Info;
        case DDSI_SHM_DEBUG : return Iceoryx_LogLevel_Debug;
        case DDSI_SHM_VERBOSE : return Iceoryx_LogLevel_Verbose;
    }
    return Iceoryx_LogLevel_Off;
}

void shm_set_loglevel(enum ddsi_shm_loglevel level) {
    iox_set_loglevel(to_iox_loglevel(level));
}
