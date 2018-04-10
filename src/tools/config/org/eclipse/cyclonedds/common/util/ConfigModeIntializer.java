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
package org.eclipse.cyclonedds.common.util;

public class ConfigModeIntializer {
    
    public static final String COMMUNITY = "COMMUNITY";
    public static final String COMMERCIAL = "COMMERCIAL";
    public static final int  COMMUNITY_MODE = 1;
    public static final int  COMMERCIAL_MODE = 2;
    public static final int  COMMUNITY_MODE_FILE_OPEN = 3;
    public static final int LITE_MODE = 4;
    public static int  CONFIGURATOR_MODE = COMMERCIAL_MODE;

    public static void setMode(int mode) {
        CONFIGURATOR_MODE = mode;
    }

    public int getMode() {
        return CONFIGURATOR_MODE;
    }

}
