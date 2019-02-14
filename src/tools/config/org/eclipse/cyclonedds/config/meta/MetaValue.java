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

public abstract class MetaValue extends MetaNode {
    Object defaultValue;
    
    public MetaValue(String doc, Object defaultValue, String dimension) {
        super(doc, dimension);
        this.defaultValue = defaultValue;
        
    }

    public Object getDefaultValue() {
        return this.defaultValue;
    }

    public abstract boolean setDefaultValue(Object defaultValue); 
    
    @Override
    public String toString(){
        return "Value (" + defaultValue.getClass().toString().substring(defaultValue.getClass().toString().lastIndexOf('.') + 1) + ") DefaultValue: " + defaultValue.toString();
    }
    
    @Override
    public boolean equals(Object object){
        boolean result;
        MetaValue mv;
        
        if(object instanceof MetaValue){
            mv = (MetaValue)object;
            if((this.defaultValue == null) || (mv.getDefaultValue() == null)){
                if(this.defaultValue != mv.getDefaultValue()){
                    result = false;
                } else {
                    result = true;
                }
            } else if(this.defaultValue.equals(mv.getDefaultValue())){
                result = true;
            } else {
                result = false;
            }
        } else {
            result = false;
        }
        return result;
    }
    @Override
    public int hashCode() {
        int var_gen_code;
        int hash = 13;
        var_gen_code = (null == defaultValue ? 0 : defaultValue.hashCode());
        hash = 31 * hash + var_gen_code;
        return hash;
    }
}
