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

public abstract class MetaValueNatural extends MetaValue {
    Object maxValue;
    Object minValue;
    
    public MetaValueNatural(String doc, Object defaultValue, Object maxValue,
            Object minValue, String dimension) {
        super(doc, defaultValue, dimension);
        this.minValue = minValue;
        this.maxValue = maxValue;
    }

    public Object getMaxValue() {
        return this.maxValue;
    }

    public Object getMinValue() {
        return this.minValue;
    }
    
    @Override
    public boolean equals(Object object){
        MetaValueNatural mn;
        boolean result = super.equals(object);
        
        if(result){
            if(object instanceof MetaValueNatural){
                mn = (MetaValueNatural)object;
                
                if((mn.getMaxValue() == null) && (this.maxValue != null)){
                    result = false;
                } else if((mn.getMaxValue() != null) && (this.maxValue == null)){
                    result = false;
                } else if( ((mn.getMaxValue() == null) && (this.maxValue == null)) ||
                           ((mn.getMaxValue().equals(this.maxValue))))
                {
                    if((mn.getMinValue() == null) && (this.minValue != null)){
                        result = false;
                    } else if((mn.getMinValue() != null) && (this.minValue == null)){
                        result = false;
                    } else if( ((mn.getMinValue() == null) && (this.minValue == null)) ||
                               ((mn.getMinValue().equals(this.minValue))))
                    {
                        result = true;
                    } else {
                        result = false;
                    }
                } else {
                    result = false;
                }
            } else {
                result = false;
            }
        }
        return result;
    }
    
    @Override
    public int hashCode() {
        int var_gen_code;
        int hash = 13;
        var_gen_code = (null == maxValue ? 0 : maxValue.hashCode());
        var_gen_code += (null == minValue ? 0 : minValue.hashCode());
        hash = 31 * hash + var_gen_code;
        return hash;
    }
    
    public abstract boolean setMaxValue(Object maxValue);
    
    public abstract boolean setMinValue(Object minValue);
    
    @Override
    public String toString(){
        String result = super.toString();
        
        result += ", MaxValue: " + maxValue.toString() + ", MinValue: " + minValue.toString();
        
        return result;
    }
}
