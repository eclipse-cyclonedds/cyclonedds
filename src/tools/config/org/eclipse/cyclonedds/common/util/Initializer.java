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
 * logging facilities according to commandline parameters and to validate
 * whether the correct Java Vitual Machine version is used.
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

            if(!result){
                 Report.getInstance().writeInfoLog("Applying default configuration.");
                result = Config.getInstance().loadDefault();
            }
        }
        else{
            result = Config.getInstance().loadDefault();
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

    /**
     * Validates whether a compatible Java virtual machine is used.
     * 
     * The version of Java must be &gt;= 1.4 and should be &gt;= 1.5.0. If the
     * used version is &st; 1.4, the application exits with an error message. If
     * 1.4 &st;= version &st; 1.5, false is returned, but the application
     * proceeds.
     * 
     * @return true, if java version &gt;= 1.5 and false otherwise.
     */
    public boolean validateJVMVersion(){
        int token;
        boolean result = true;

        String version = System.getProperty("java.version");
        StringTokenizer tokenizer = new StringTokenizer(version, ".");

	if(tokenizer.hasMoreTokens()){
             token = Integer.parseInt(tokenizer.nextToken());

            if(token < 1){
                this.printVersionErrorAndExit(version);
            }

            if(tokenizer.hasMoreTokens()){
                token = Integer.parseInt(tokenizer.nextToken());

                if(token < 4){
                    this.printVersionErrorAndExit(version);
                } else if(token == 4){
                    result = false;
                }
            }
            else{
                this.printVersionErrorAndExit(version);
                result = false;
            }
        }
        else{
            this.printVersionErrorAndExit(version);
            result = false;
        }
        return result;
    }

    /**
     * Prints JVM version demands as well as the used version and exits the
     * application.
     */
    private void printVersionErrorAndExit(String version){
        System.err.println("Your Java version is '" + version + "', but version >= '1.4' is required.\nBailing out...");
        System.exit(0);
    }

    /**
     * Displays a Java version warning.
     * 
     * @param parent
     *            The GUI parent which must be used as parent for displaying the
     *            message. If the supplied component == null, the version
     *            warning is displayed on standard out (System.out).
     */
    public void showVersionWarning(Component parent){
        if(parent != null){
            JOptionPane.showMessageDialog(parent, "You are using Java version " + System.getProperty("java.version") + ",\nbut version >= 1.5.0 is recommended.", "Warning", JOptionPane.WARNING_MESSAGE);
        } else{
            System.err.println("You are using Java version " + System.getProperty("java.version")
                    + ",\nbut version >= 1.5.0 is recommended.");
        }
    }
}
