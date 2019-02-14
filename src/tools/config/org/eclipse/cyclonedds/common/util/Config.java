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
/**
 * Contains all SPLICE DDS C&M Tooling utilities.
 */
package org.eclipse.cyclonedds.common.util;

import java.awt.Color;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.Iterator;
import java.util.Properties;

import org.eclipse.cyclonedds.common.view.CommonFileChooser;

/**
 * Supplies a generic configuration utility for all kinds of applications.
 * Before using the load or loadDefault function must be called.
 * 
 * @date Oct 25, 2004
 */
public class Config {

    /**
     * The current configuration.
     */
    private Properties config = null;

    /**
     * The location of the configuration.
     */
    private File configFile = null;

    /**
     * The validator.
     */
    private ConfigValidator validator = null;

    private CommonFileChooser chooser = null;

    private static volatile Config instance       = null;

    private static Color warningColor = null;
    private static Color errorColor = null;
    private static Color sensitiveColor = null;
    private static Color inactiveColor = null;
    private static Color activeColor = null;
    private static Color inputColor = null;
    private static Color incorrectColor = null;

    private Config(){}

    public static Config getInstance(){
        if(instance == null){
            instance = new Config();
        }
        return instance;
    }

    /**
     * Assigns the supplied validator to the configuration. It will use this
     * validator when calling: - load --> call validateValue on each found key.
     * - loadDefault --> call validateValue on each found key. - getProperty ->
     * call getDefaultValue when a supplied kay has no value. - setProperty -->
     * call isValueValid on the supplied key/value combination.
     * 
     * @param _validator
     *            The validator to assign.
     */
    public void setValidator(ConfigValidator _validator){
        validator = _validator;
    }

    /**
     * Provides access to the current configuration validator.
     * 
     * @return The current validator or null if none has been assigned.
     */
    public ConfigValidator getValidator(){
        return validator;
    }

    /**
     * Provides access to the current configuration properties.
     * 
     * @return The current config properties or null if none has been assigned.
     */
    public Properties getConfig() {
        return config;
    }

    /**
     * Loads the configuration from the supplied URI.
     * 
     * @param uri
     *            The URI to load the configuration from.
     * @return true if the configuration was successfully loaded, false
     *         otherwise.
     */
    public boolean load(String uri) {
        String value;
        boolean result = false;
        FileInputStream fis = null;

        try {
            uri = uri.replaceAll(" ", "%20");
            URI location = new URI(uri);
            configFile = new File(location);
            config = new Properties();
            fis = new FileInputStream(configFile);
            config.load(fis);
            Properties newConfig = new Properties();
            if(validator != null){

                for (Object key : config.keySet()) {
                    value = config.getProperty((String) key);
                    value = validator.getValidatedValue((String) key, value);
                    if (value != null) {
                        newConfig.setProperty((String) key, value);
                    }
                }
            }
            config.putAll(newConfig);
            result = true;
        } catch (FileNotFoundException e1) {
            Report.getInstance().writeErrorLog("Configuration file could not be found.");
        } catch (IOException e1) {
            Report.getInstance().writeErrorLog("Configuration file could not be read.");
        } catch (URISyntaxException e) {
            Report.getInstance().writeErrorLog("Supplied URI not valid.");
        } finally {
            if (fis != null) {
                try {
                    fis.close();
                } catch (IOException ie) {
                    Report.getInstance().writeErrorLog("Configuration file could not be read.");
                }
            }
        }
        return result;
    }

    /**
     * Provides access to the value of the supplied property.
     * 
     * @param key
     *            The name of the property.
     * @return The value of the property or null if it was not available.
     */
    public String getProperty(String key){
        String result = null;

        if(config != null){
            if(config.containsKey(key)){
                result = config.getProperty(key);
            } else if(validator != null){
                result = validator.getDefaultValue(key);

                if(result != null){
                    config.setProperty(key, result);
                }
            }
        }
        return result;
    }

    /**
     * Assigns the supplied value to the supplied property.
     * 
     * @param key
     *            The name of the property.
     * @param value
     *            The value of the property.
     * @return true if it could be assigned, false otherwise.
     */
    public boolean setProperty(String key, String value){
        boolean result = false;

        if(config != null){
            if(validator != null){
                result = validator.isValueValid(key, value);

                if(result){
                    config.setProperty(key, value);
                    this.store();
                }
            } else {
                config.setProperty(key, value);
                this.store();
            }
            result = true;
        }
        return result;
    }

    public boolean isPropertyValid(String key, String value){
        boolean result = false;

        if((config != null) && (validator != null)){
            result = validator.isValueValid(key, value);
        }
        return result;
    }

    /**
     * Removes the supplied property from the configuration.
     * 
     * @param key
     *            The name of the property to remove.
     * @return true if succeeded, false otherwise.
     */
    public boolean removeProperty(String key){
        boolean result = false;

        if(config != null){
            Object value = config.remove(key);

            if(value != null){
                result = true;
            }
        }
        return result;
    }

    /**
     * Prints the configuration to screen.
     */
    public void list(){
        if(config != null){
            config.list(System.out);
        }
    }

    /**
     * Stores the configuration to disk.
     * 
     * @return true if succeeded, false otherwise.
     */
    public boolean store(){
        boolean result = false;
        FileOutputStream fos = null;

        if(config != null){
            try {
                fos = new FileOutputStream(configFile);
                config.store(fos, null);
                result = true;
            } catch (IOException e) {
                Report.getInstance().writeErrorLog("Configuration could not be saved.");
            } finally {
                if (fos != null) {
                    try {
                        fos.close();
                    } catch (IOException ie) {
                        Report.getInstance().writeErrorLog("Configuration could not be saved.");
                    }
                }
            }
        }
        return result;
    }

    /**
     * Stores the configuration to the supplied uri.
     * 
     * @return true if succeeded, false otherwise.
     */
    public boolean store(String uri){
        boolean result = false;
        FileOutputStream fos = null;

        if(config != null){
            try {
                Report.getInstance().writeInfoLog("Storing configuration to URI: " +
                        configFile.toURI().getScheme() +
                        configFile.toURI().getPath() + ".");
                uri = uri.replaceAll(" ", "%20");
                URI outURI = new URI(uri);
                File outFile = new File(outURI);
                boolean fileCreated = true;

                if(!(outFile.exists())){
                    fileCreated = outFile.createNewFile();
                }
                if (fileCreated) {
                    fos = new FileOutputStream(outFile);
                    config.store(fos, null);
                } else {
                    Report.getInstance().writeErrorLog("Configuration could not be saved. file could not be created");
                    result = false;
                }
            } catch (IOException e) {
                Report.getInstance().writeErrorLog("Configuration could not be saved.");
            } catch (URISyntaxException e) {
                Report.getInstance().writeErrorLog("Configuration could not be saved.");
            } finally {
                if (fos != null) {
                    try {
                        fos.close();
                    } catch (IOException ie) {
                        Report.getInstance().writeErrorLog("Configuration could not be saved.");
                    }
                }
            }
        }
        return result;
    }

    public CommonFileChooser getFileChooser(){
        if(this.chooser == null){
            this.chooser = new CommonFileChooser(".");
        }
        return this.chooser;
    }

    public static Color getErrorColor(){
        if(errorColor == null){
            int r = Integer.parseInt("a0", 16);
            int g = Integer.parseInt("20", 16);
            int b = Integer.parseInt("20", 16);

            r = Integer.parseInt("ff", 16);
            g = Integer.parseInt("40", 16);
            b = Integer.parseInt("40", 16);

            errorColor = new Color(r, g, b);
        }
        return errorColor;
    }

    public static Color getWarningColor(){
        if(warningColor == null){
            int r = Integer.parseInt("c0", 16);
            int g = Integer.parseInt("80", 16);
            int b = Integer.parseInt("00", 16);
            warningColor = new Color(r, g, b);
        }
        return warningColor;
    }

    public static Color getSensitiveColor(){
        if(sensitiveColor == null){
            int r = Integer.parseInt("70", 16);
            int g = Integer.parseInt("e0", 16);
            int b = Integer.parseInt("70", 16);
            sensitiveColor = new Color(r, g, b);
        }
        return sensitiveColor;
    }

    public static Color getInactiveColor(){
        if(inactiveColor == null){
            int r = Integer.parseInt("70", 16);
            int g = Integer.parseInt("70", 16);
            int b = Integer.parseInt("7a", 16);
            inactiveColor = new Color(r, g, b);
        }
        return inactiveColor;
    }

    public static Color getActiveColor(){
        if(activeColor == null){
            int r = Integer.parseInt("50", 16);
            int g = Integer.parseInt("90", 16);
            int b = Integer.parseInt("50", 16);
            activeColor = new Color(r, g, b);
        }
        return activeColor;
    }

    public static Color getInputColor(){
        if(inputColor == null){
            int r = Integer.parseInt("f0", 16);
            int g = Integer.parseInt("90", 16);
            int b = Integer.parseInt("00", 16);
            inputColor = new Color(r, g, b);
        }
        return inputColor;
    }

    public static Color getIncorrectColor(){
        if(incorrectColor == null){
            int r = Integer.parseInt("ff", 16);
            int g = Integer.parseInt("40", 16);
            int b = Integer.parseInt("40", 16);
            incorrectColor = new Color(r, g, b);
        }
        return incorrectColor;
    }
}
