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
package org.eclipse.cyclonedds.config.meta;

public class MetaValueLong extends MetaValueNatural {
    public MetaValueLong(String doc, Long defaultValue, Long maxValue,
            Long minValue, String dimension) {
        super(doc, defaultValue, maxValue, minValue, dimension);
    }

    @Override
    public boolean setMaxValue(Object maxValue) {
        boolean result;
        
        if(maxValue instanceof Long){
            this.maxValue = maxValue;
            result = true;
        } else {
            result = false;
        }
        return result;
    }

    @Override
    public boolean setMinValue(Object minValue) {
        boolean result;
        
        if(minValue instanceof Long){
            this.minValue = minValue;
            result = true;
        } else {
            result = false;
        }
        return result;
    }

    @Override
    public boolean setDefaultValue(Object defaultValue) {
        boolean result;
        
        if(defaultValue instanceof Long){
            this.defaultValue = defaultValue;
            result = true;
        } else {
            result = false;
        }
        return result;
    }
}
