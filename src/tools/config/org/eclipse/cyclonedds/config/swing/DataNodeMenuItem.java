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
package org.eclipse.cyclonedds.config.swing;

import javax.swing.JMenuItem;

import org.eclipse.cyclonedds.config.data.DataNode;
import org.eclipse.cyclonedds.config.meta.MetaNode;

public class DataNodeMenuItem extends JMenuItem {
    private static final long serialVersionUID = -5909316541978936911L;
    private DataNode data;
    private MetaNode childMeta;
    private DataNodePopupSupport source;
    
    public DataNodeMenuItem(String name, DataNode data, MetaNode childMeta){
        super(name);
        this.data = data;
        this.childMeta = childMeta;
        this.source = null;
    }
    
    public DataNodeMenuItem(String name, DataNode data, MetaNode childMeta, DataNodePopupSupport source){
        super(name);
        this.data = data;
        this.childMeta = childMeta;
        this.source = source;
    }
    
    public DataNode getData(){
        return this.data;
    }
    
    public MetaNode getChildMeta(){
        return this.childMeta;
    }
    
    public DataNodePopupSupport getSource(){
        return this.source;
    }
}
