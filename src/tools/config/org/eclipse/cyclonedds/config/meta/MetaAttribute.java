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

public class MetaAttribute extends MetaNode {
    private String name;
    private boolean required;
    private MetaValue value;
    
    public MetaAttribute(String doc, String name, boolean required,
            MetaValue value, String dimension) {
        super(doc, dimension);
        this.name = name;
        this.required = required;
        this.value = value;
    }
    
    public boolean isRequired(){
        return this.required;
    }

    public String getName() {
        return this.name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public MetaValue getValue() {
        return this.value;
    }

    public void setValue(MetaValue value) {
        this.value = value;
    }
    
    @Override
    public boolean equals(Object object){
        MetaAttribute ma;
        boolean result;
        
        if(object instanceof MetaAttribute){
            ma = (MetaAttribute)object;
            
            if(this.name.equals(ma.getName())){
                if(this.required == ma.isRequired()){
                    if(this.value.equals(ma.getValue())){
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
        } else {
            result = false;
        }
        return result;
    }
    
    @Override
    public int hashCode() {
        int var_gen_code;
        int hash = 13;
        var_gen_code = required ? 1 : 0;
        var_gen_code += (null == value ? 0 : value.hashCode());
        var_gen_code += (null == name ? 0 : name.hashCode());
        hash = 31 * hash + var_gen_code;
        return hash;
    }
    
    @Override
    public String toString(){
        String result = "";
        result += "\nAttribute\n";
        result += "-Name: " + this.name + "\n";
        result += "-Required: " + this.required + "\n";
        result += "-Value: " + value.toString();
        
        return result;
    }
}
