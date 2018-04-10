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

import java.awt.Component;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.font.FontRenderContext;

import javax.swing.JLabel;
import javax.swing.JPopupMenu;
import javax.swing.JTree;
import javax.swing.UIManager;
import javax.swing.tree.DefaultMutableTreeNode;
import javax.swing.tree.DefaultTreeCellRenderer;
import javax.swing.tree.DefaultTreeModel;
import javax.swing.tree.TreeCellRenderer;
import javax.swing.tree.TreePath;
import javax.swing.tree.TreeSelectionModel;

import org.eclipse.cyclonedds.common.view.StatusPanel;
import org.eclipse.cyclonedds.config.data.DataConfigurationListener;
import org.eclipse.cyclonedds.config.data.DataElement;
import org.eclipse.cyclonedds.config.data.DataNode;
import org.eclipse.cyclonedds.config.data.DataValue;
import org.eclipse.cyclonedds.config.meta.MetaElement;

public class DataElementTree extends JTree 
    implements DataConfigurationListener, TreeCellRenderer, DataNodePopupSupport 
{
    private static final long serialVersionUID = 5103341138480897742L;
    private final DataElement rootElement;
    private final DefaultMutableTreeNode rootNode;
    private final DefaultTreeModel model;
    private final DefaultTreeCellRenderer initialRenderer;
    private DataNodePopup popupController;
    private final StatusPanel status;
    
    public DataElementTree(DataElement rootElement, DataNodePopup popup, StatusPanel status){
        super();
        this.initialRenderer = new DefaultTreeCellRenderer();
        this.setCellRenderer(this);
        this.getSelectionModel().setSelectionMode(TreeSelectionModel.SINGLE_TREE_SELECTION);
        this.setEditable(false);
        this.setRootVisible(true);
        this.setShowsRootHandles(true);
        this.rootElement = rootElement;
        this.status = status;
        this.rootElement.getOwner().addDataConfigurationListener(this);
        this.rootNode = new DefaultMutableTreeNode(this.rootElement);
        this.model = ((DefaultTreeModel)this.treeModel);
        this.model.setRoot(this.rootNode);
        this.initElement(this.rootNode, this.rootElement);
        if(this.rootNode.getChildCount() > 0){
            this.scrollPathToVisible(
                    new TreePath(
                            ((DefaultMutableTreeNode)this.rootNode.getFirstChild()).getPath()));
        }
        this.setSelectionPath(new TreePath(this.rootNode.getPath()));
        
        if(popup == null){
            this.popupController = new DataNodePopup();
        } else {
            this.popupController = popup;
        }
        this.popupController.registerPopupSupport(this);
    }
    
    private void initElement(DefaultMutableTreeNode parent, DataElement element){
        DefaultMutableTreeNode node;
        
        for(DataNode child: element.getChildren()){
            if(child instanceof DataElement){
                if(((MetaElement)child.getMetadata()).hasElementChildren()){
                    node = new DefaultMutableTreeNode(child);
                    this.model.insertNodeInto(node, parent, parent.getChildCount());
                    this.initElement(node, (DataElement)child);
                } else if(!((MetaElement)child.getMetadata()).hasValueChildren()){
                    node = new DefaultMutableTreeNode(child);
                    this.model.insertNodeInto(node, parent, parent.getChildCount());
                    this.initElement(node, (DataElement)child);
                }
            }
        }
    }

    @Override
    public void nodeAdded(DataElement parent, DataNode nodeAdded) {
        DefaultMutableTreeNode parentNode, childNode;
        
        if(nodeAdded instanceof DataElement){
            parentNode = this.lookupNode(this.rootNode, parent);
            
            if(parentNode != null){
                if(((MetaElement)nodeAdded.getMetadata()).hasElementChildren()){
                    childNode = new DefaultMutableTreeNode(nodeAdded);
                    this.model.insertNodeInto(childNode, parentNode, parentNode.getChildCount());
                    this.scrollPathToVisible(new TreePath(childNode.getPath()));
                    this.initElement(childNode, (DataElement)nodeAdded);
                } else if(!((MetaElement)nodeAdded.getMetadata()).hasValueChildren()){
                    childNode = new DefaultMutableTreeNode(nodeAdded);
                    this.model.insertNodeInto(childNode, parentNode, parentNode.getChildCount());
                    this.scrollPathToVisible(new TreePath(childNode.getPath()));
                    this.initElement(childNode, (DataElement)nodeAdded);
                }
            }
        }
        this.repaint();
    }

    @Override
    public void nodeRemoved(DataElement parent, DataNode nodeRemoved) {
        DefaultMutableTreeNode node;
        
        if(nodeRemoved instanceof DataElement){
            node = this.lookupNode(this.rootNode, (DataElement)nodeRemoved);
            
            if(node != null){
                if(node.getParent() != null){
                    this.setSelectionPath(new TreePath(((DefaultMutableTreeNode)node.getParent()).getPath()));
                }
                if(node.getParent() != null){
                    this.model.removeNodeFromParent(node);
                }
            }
        }
        this.repaint();
    }

    @Override
    public void valueChanged(DataValue data, Object oldValue, Object newValue) {
        this.repaint();
    }
    
    private DefaultMutableTreeNode lookupNode(DefaultMutableTreeNode node, DataElement element){
        DefaultMutableTreeNode result = null;
        
        if(node.getUserObject().equals(element)){
            result = node;
        } else {
            for(int i=0; (i<node.getChildCount()) && (result == null); i++){
                result = this.lookupNode((DefaultMutableTreeNode)node.getChildAt(i), element);
            }
        }
        return result;
    }

    @Override
    public Component getTreeCellRendererComponent(JTree tree, Object value, boolean selected, boolean expanded, boolean leaf, int row, boolean hasFocus) {
        Component result = null;
        JLabel temp;
        Object nodeValue;
        
        if(value instanceof DefaultMutableTreeNode){
            nodeValue = ((DefaultMutableTreeNode)value).getUserObject();
            
            if(nodeValue instanceof DataElement){
                temp = new JLabel(ConfigUtil.getExtendedDataElementString((DataElement)nodeValue));
                
                if(selected){
                    temp.setForeground(UIManager.getColor("Tree.selectionForeground"));
                    temp.setBackground(UIManager.getColor("Tree.selectionBackground"));
                    temp.setOpaque(true);                   
                } else {
                    temp.setForeground(UIManager.getColor("Tree.textForeground"));
                    temp.setBackground(UIManager.getColor("Tree.textBackground"));
                }
                temp.setFont(temp.getFont().deriveFont(Font.BOLD));
                Dimension dim = temp.getPreferredSize();
                
                if(dim != null){
                    FontRenderContext frc = new FontRenderContext(temp.getFont().getTransform(), false, false);
                    double width = temp.getFont().getStringBounds(temp.getText(), frc).getWidth();
                    dim.width = (int)width + 3;
                    temp.setPreferredSize(dim);
                    temp.setMinimumSize(dim);
                    
                }
                temp.setComponentOrientation(this.getComponentOrientation());
                
                result = temp;
            }
        }
        
        if(result == null){
            result = initialRenderer.getTreeCellRendererComponent(tree, value, selected, expanded, leaf, row, hasFocus);
        }
        return result;
    }

    @Override
    public DataNode getDataNodeAt(int x, int y) {
        DataNode retVal = null;
        TreePath path = this.getClosestPathForLocation(x, y);

        if (path != null) {
            this.setSelectionPath(path);
            retVal = (DataElement)((DefaultMutableTreeNode)path.getLastPathComponent()).getUserObject();
        }
        return retVal;
    }

    @Override
    public void showPopup(JPopupMenu popup, int x, int y) {
        popup.show(this, x, y);
    }

    @Override
    public void setStatus(String message, boolean persistent, boolean busy) {
        if(this.status != null){
            this.status.setStatus(message, persistent, busy);
        }
    }
}
