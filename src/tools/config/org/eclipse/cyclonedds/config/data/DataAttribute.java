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
package org.eclipse.cyclonedds.config.data;

import org.w3c.dom.Attr;

import org.eclipse.cyclonedds.config.meta.MetaAttribute;

public class DataAttribute extends DataNode {
    private DataValue value;
    
    public DataAttribute(MetaAttribute metadata, Attr node, Object value) throws DataException {
        super(metadata, node);
        
        if(!node.getNodeName().equals(metadata.getName())){
            throw new DataException("Metadata and data do not match.");
        }
        this.value = new DataValue(metadata.getValue(), node, value);
        this.value.setParent(this);
        this.value.setOwner(this.getOwner());
    }
    
    public DataAttribute(MetaAttribute metadata, Attr node) throws DataException {
        super(metadata, node);
        
        if(!node.getNodeName().equals(metadata.getName())){
            throw new DataException("Metadata and data do not match.");
        }
        Object defaultValue = metadata.getValue().getDefaultValue();
        this.value = new DataValue(metadata.getValue(), node, defaultValue);
        this.value.setParent(this);
        this.value.setOwner(this.getOwner());
    }
    
    @Override
    public void setOwner(DataConfiguration owner) {
        super.setOwner(owner);
        this.value.setOwner(owner);
    }
    
    public DataValue getDataValue(){
        return this.value;
    }
    
    public void testSetValue(Object value) throws DataException{
        this.value.testSetValue(value);
    }

    public void setValue(Object value) throws DataException{
        DataValue tmpValue = this.value;
        this.value.setValue(value);
        tmpValue.setParent(null);
        tmpValue.setOwner(null);
        this.value.setParent(this);
        this.value.setOwner(this.owner);
    }
    
    public Object getValue(){
        return this.value.getValue();
    }
}
