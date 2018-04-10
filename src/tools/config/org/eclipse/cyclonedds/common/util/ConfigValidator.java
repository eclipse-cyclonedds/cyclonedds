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

/**
 * Interface that provides routines for validating configuration. 
 * Implementations of this interface can guarantee that the configuration for 
 * an application will be correct.
 * 
 * @date Jan 12, 2005 
 */
public interface ConfigValidator {
    /**
     * Returns the correct value for the supplied key. This function
     * checks if the supplied value is valid. If so, return that value. If not
     * return a valid one. The Config component calls this routine for each
     * key it finds when loading a configuration file.
     * 
     * @param key The key in the configuration.
     * @param value The value as it has been found in the confuration file.
     * @return A correct value for that key. If null is returned, the Config 
     *         component will remove the key from the configuration.
     */
    public String getValidatedValue(String key, String value);
 
    /**
     * Returns the default value for the supplied key. This function is called
     * by the Config component when an application asks for a property that has
     * not been defined.
     * 
     * @param key The key where to resolve the default value of. 
     * @return The default value for the supplied key.
     */
    public String getDefaultValue(String key);
    
    /**
     * Checks whether the supplied key/value combination is valid. The Config
     * component calls this function when an application sets a property in the
     * configuration.
     * 
     * @param key The key of the property.
     * @param value The value of the property.
     * @return If the combination is valid; true and false otherwise.
     */
    public boolean isValueValid(String key, String value);
}
