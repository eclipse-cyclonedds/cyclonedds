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

import java.util.HashSet;

import org.eclipse.cyclonedds.config.meta.MetaNode;
import org.w3c.dom.Node;

public abstract class DataNode {
    protected MetaNode metadata;
    protected Node node;
    protected DataNode parent;
    protected DataConfiguration owner;
    private HashSet<DataNode>   dependencies = null;
    
    public DataNode(MetaNode metadata, Node node) throws DataException {
        if(metadata == null){
            throw new DataException("Invalid metadata.");
        } else if(node == null){
            throw new DataException("Invalid data.");
        }
        this.metadata = metadata;
        this.node     = node;
        this.parent   = null;
        this.owner    = null;
    }

    public void addDependency(DataNode dv) {
        if (dependencies == null) {
            dependencies = new HashSet<DataNode>();
        }
        dependencies.add(dv);
    }

    public HashSet<DataNode> getDependencies() {
        return dependencies;
    }

    public MetaNode getMetadata() {
        return this.metadata;
    }

    public Node getNode() {
        return this.node;
    }

    public DataConfiguration getOwner() {
        return this.owner;
    }

    public void setOwner(DataConfiguration owner) {
        this.owner = owner;
    }
    
    protected void setParent(DataNode node){
        this.parent = node;
    }
    
    public DataNode getParent(){
        return this.parent;
    }
    
}
