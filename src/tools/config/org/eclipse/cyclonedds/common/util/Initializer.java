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

import java.awt.Component;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.StringTokenizer;
import java.util.logging.LogManager;

import javax.swing.JOptionPane;

/**
 * Base class for tooling initializers. Its responsibilities are to initialize
 * logging facilities according to commandline parameters.
 * 
 * @date Sep 1, 2004
 */
public class Initializer {

    /**
     * Initializes the application, using the configuration that is supplied as
     * argument. If the supplied file does not exist or no argument was
     * supplied, the default is used. The default is:
     * <USER_HOME>/.splice_tooling.properties.
     * 
     * @param args
     *            The list of arguments supplied by the user. Only the first
     *            argument is used, the rest will be ignored. The first argument
     *            must supply the location of a java properties file.
     */
    public void initializeConfig(String[] args, ConfigValidator validator){
        boolean result = false;

        Config.getInstance().setValidator(validator);

        if(args.length > 0){
             Report.getInstance().writeInfoLog("Reading configuration from " + args[0] + ".");
            result = Config.getInstance().load(args[0]);
        }

        if(!result){
             Report.getInstance().writeInfoLog("Default configuration could not be read.");
        } else {
            String loggingFileName = Config.getInstance().getProperty("logging");

            if(loggingFileName != null){
                FileInputStream is = null;
                try {
                    is = new FileInputStream(loggingFileName);
                    LogManager.getLogManager().readConfiguration(is);
                }
                catch (FileNotFoundException e) {
                     Report.getInstance().writeInfoLog("Specified logging config file not found. Logging is disabled.");
                    LogManager.getLogManager().reset();
                }
                catch (SecurityException e) {
                     Report.getInstance().writeInfoLog("Specified logging config file not valid. Logging is disabled.");
                    LogManager.getLogManager().reset();
                }
                catch (IOException e) {
                     Report.getInstance().writeInfoLog("Specified logging config file not valid. Logging is disabled.");
                    LogManager.getLogManager().reset();
                } finally {
                    if (is != null) {
                        try {
                            is.close();
                        } catch (IOException ie) {
                             Report.getInstance().writeInfoLog("Specified logging config file not valid. Logging is disabled.");
                            LogManager.getLogManager().reset();
                        }
                    }
                }
            } else {
                LogManager.getLogManager().reset();
            }
        }
    }
}
