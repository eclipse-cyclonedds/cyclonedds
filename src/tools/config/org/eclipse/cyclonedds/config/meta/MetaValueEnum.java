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

import java.util.ArrayList;

public class MetaValueEnum extends MetaValue {
    private ArrayList<String> posValues;
    
    public MetaValueEnum(String doc, String defaultValue,
            ArrayList<String> posValues, String dimension) {
        super(doc, defaultValue, dimension);
        assert(defaultValue != null);
        
        this.posValues = new ArrayList<String>();
        
        if(posValues != null){
            this.posValues.addAll(posValues);
        }
        
        if(!this.posValues.contains(defaultValue)){
            this.posValues.add(defaultValue);
        }
    }

    public String[] getPosValues() {
        return this.posValues.toArray(new String[this.posValues.size()]);
    }

    public boolean setPosValues(ArrayList<String> posValues) {
        boolean result;
        
        if((posValues != null) && (posValues.size() > 0)){
            this.posValues.clear();
            this.posValues.addAll(posValues);
            
            if(!posValues.contains(defaultValue)){
                this.defaultValue = this.posValues.get(0);
            }
            result = true;
        } else {
            result = false;
        }
        return result;
    }
    
    public boolean addPosValue(String value){
        return this.posValues.add(value);
    }

    public boolean removePosValue(String value){
        boolean result;
        
        if(!defaultValue.equals(value)){
            result = this.posValues.remove(value);
        } else {
            result = false;
        }
        return result;
    }
    
    @Override
    public boolean setDefaultValue(Object defaultValue) {
        boolean result = false;
        
        if(defaultValue instanceof String){
            this.defaultValue = defaultValue;
            
            if(!this.posValues.contains(defaultValue)){
                result = this.addPosValue((String)defaultValue);
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
        var_gen_code += (null == posValues ? 0 : posValues.hashCode());
        hash = 31 * hash + var_gen_code;
        return hash;
    }
}
