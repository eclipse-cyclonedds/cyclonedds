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

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;

import javax.swing.JMenu;
import javax.swing.JOptionPane;
import javax.swing.JPopupMenu;

import org.eclipse.cyclonedds.common.util.Report;
import org.eclipse.cyclonedds.config.data.DataAttribute;
import org.eclipse.cyclonedds.config.data.DataElement;
import org.eclipse.cyclonedds.config.data.DataException;
import org.eclipse.cyclonedds.config.data.DataNode;
import org.eclipse.cyclonedds.config.data.DataValue;
import org.eclipse.cyclonedds.config.meta.MetaAttribute;
import org.eclipse.cyclonedds.config.meta.MetaElement;
import org.eclipse.cyclonedds.config.meta.MetaNode;
import org.eclipse.cyclonedds.config.meta.MetaValue;

public class DataNodePopup implements MouseListener, ActionListener {

    private final Set<DataNodePopupSupport> popupSupport;

    public DataNodePopup() {
        this.popupSupport = Collections.synchronizedSet(new HashSet<DataNodePopupSupport>());
    }

    public void unregisterPopupSupport(DataNodePopupSupport support){
        synchronized(this.popupSupport){
            if(this.popupSupport.contains(support)){
                support.removeMouseListener(this);
                this.popupSupport.remove(support);
            }
        }
    }

    public void registerPopupSupport(DataNodePopupSupport support){
        synchronized(this.popupSupport){
            if(!this.popupSupport.contains(support)){
                support.addMouseListener(this);
                this.popupSupport.add(support);
            }
        }
    }

    private void notifyStatus(String message, boolean persistent, boolean busy){
        synchronized(this.popupSupport){
            for(DataNodePopupSupport dp: this.popupSupport){
                dp.setStatus(message, persistent, busy);
            }
        }
    }

    private int countOccurrences(DataElement parent, MetaNode metaChild){
        int occurrences = 0;

        for(DataNode child: parent.getChildren()){
            if(metaChild.equals(child.getMetadata())){
                occurrences++;
            }
        }
        return occurrences;
    }

    private JPopupMenu getPopup(DataNodePopupSupport source, DataNode dn){
        DataNode parent;
        DataElement parentParent;
        DataNodeMenuItem reset, remove, add;
        MetaNode metaParent, meta;
        JMenu addMenu;
        int min, max, current;

        JPopupMenu result = new JPopupMenu("Actions");

        if (dn != null) {
            parent     = dn.getParent();
            meta       = dn.getMetadata();
        } else {
            parent = null;
            meta = null;
        }

        if(parent != null){
            metaParent = parent.getMetadata();
        } else {
            metaParent = null;
        }
        addMenu    = null;
        reset      = null;
        remove     = null;

        if(dn instanceof DataValue){
            assert metaParent!=null: "Parent element == null";
            reset = new DataNodeMenuItem("Reset to default", dn, null);
            remove = new DataNodeMenuItem("Remove", parent, null);

            if(parent instanceof DataElement){
                addMenu = new JMenu("Add");

                for(MetaNode child: ((MetaElement)metaParent).getChildren()){
                    if(child instanceof MetaAttribute){
                        current = this.countOccurrences((DataElement)parent, child);
                        add = new DataNodeMenuItem(((MetaAttribute)child).getName(), parent, child);

                        if(current == 1){
                            add.setEnabled(false);
                        } else {
                            add.setEnabled(true);
                        }
                        add.setActionCommand("add");
                        add.addActionListener(this);
                        addMenu.add(add);
                    }
                }

                parentParent = (DataElement)parent.getParent();

                if(parentParent != null){
                    current = this.countOccurrences((DataElement)parent.getParent(), parent.getMetadata());
                    min = ((MetaElement)metaParent).getMinOccurrences();
                    if(min == 0){
                        remove.setEnabled(true);
                    } else if(current > min){
                        remove.setEnabled(true);
                    } else {
                        remove.setEnabled(false);
                    }
                }
            } else if(parent instanceof DataAttribute){
                metaParent = parent.getMetadata();
                if(((MetaAttribute)metaParent).isRequired()){
                    remove.setEnabled(false);
                }
                result.add(remove);
            } else {
                assert false: "Unexpected parent type found: " + (parent==null ? parent: parent.getClass());
            }
        } else if(dn instanceof DataElement){
            addMenu = new JMenu("Add");
            remove = new DataNodeMenuItem("Remove", dn, null);

            min = ((MetaElement)meta).getMinOccurrences();
            current = this.countOccurrences((DataElement)dn.getParent(), meta);

            if(min < current){
                remove.setEnabled(true);
            } else {
                remove.setEnabled(false);
            }

            for(MetaNode child: ((MetaElement)meta).getChildren()){
                current = this.countOccurrences((DataElement)dn, child);

                if(child instanceof MetaElement){
                    add = new DataNodeMenuItem(((MetaElement)child).getName(), dn, child);
                    max = ((MetaElement)child).getMaxOccurrences();

                    if((max == 0) || (current < max)){
                        add.setEnabled(true);
                    } else {
                        add.setEnabled(false);
                    }

                } else if(child instanceof MetaAttribute){
                    add = new DataNodeMenuItem(((MetaAttribute)child).getName(), dn, child);

                    if(current == 1){
                        add.setEnabled(false);
                    } else {
                        add.setEnabled(true);
                    }
                } else {
                    add = new DataNodeMenuItem("<NoName>", dn, child);

                    if(current == 1){
                        add.setEnabled(false);
                    } else {
                        add.setEnabled(true);
                    }
                }
                add.setActionCommand("add");
                add.addActionListener(this);
                addMenu.add(add);
            }

        }
        if(addMenu != null){
        	if (addMenu.getItemCount() == 0){
        		addMenu.setEnabled(false);
        	}
            result.add(addMenu);

        }
        if(remove != null){
            remove.setActionCommand("remove");
            remove.addActionListener(this);
            result.add(remove);
        }

        if(reset != null){
            result.addSeparator();
            reset.setActionCommand("reset");
            reset.addActionListener(this);
            result.add(reset);
        } else {
            //result.addSeparator();
        }
        return result;
    }
    @Override
    public void actionPerformed(ActionEvent e) {
        DataNodeMenuItem item;
        DataNode dataNode;
        MetaNode metaNode;
        DataElement dataDomain = null;
        Object source = e.getSource();
        String command = e.getActionCommand();

        try{
            if(source instanceof DataNodeMenuItem){
                item = (DataNodeMenuItem)source;
                if("remove".equals(command)){
                    dataNode = item.getData();
                    boolean remove = false;
                    if (dataNode.getOwner().isServiceElement(dataNode)) {
                        int value = JOptionPane.showConfirmDialog(null,
                                "Are you sure you want to remove this " + ConfigWindow.SERVICE_TEXT + "?", "Remove " + ConfigWindow.SERVICE_TEXT + " confirmation",
                                JOptionPane.YES_NO_OPTION);
                        if (value == JOptionPane.NO_OPTION) {
                            remove = true;
                        }
                    }
                    if (!remove && dataNode.getDependencies() != null) {
                        for (DataNode dn : dataNode.getDependencies()) {
                            if(dn instanceof DataElement){
                                if (dn.getOwner().isServiceElement(dn)) {
                                    int value = JOptionPane.showConfirmDialog(null,
                                            "Are you sure you want to remove this " + ConfigWindow.SERVICE_TEXT + "?",
                                            "Remove " + ConfigWindow.SERVICE_TEXT + " confirmation", JOptionPane.YES_NO_OPTION);
                                    if (value == JOptionPane.NO_OPTION) {
                                        remove = true;
                                    }
                                }
                                if (!remove) {
                                    dn.getOwner().removeNode(dn);
                                }
                            }
                        }
                    }
                    if (!remove) {
                        dataNode.getOwner().removeNode(dataNode);
                    }
                } else if ("removeService".equals(command)) {
                    boolean remove = false;
                    int value = JOptionPane.showConfirmDialog(null,
                            "Are you sure you want to remove this " + ConfigWindow.SERVICE_TEXT + "?",
                            "Remove " + ConfigWindow.SERVICE_TEXT + " confirmation", JOptionPane.YES_NO_OPTION);
                    if (value == JOptionPane.NO_OPTION) {
                        remove = true;
                    }
                    if (!remove) {
                        dataNode = item.getData();
                        dataDomain = dataNode.getOwner().findServiceDataElement(dataNode.getOwner().getRootElement(),
                                "Service", (DataElement) dataNode);
                        if (dataDomain != null) {
                            dataDomain.getOwner().removeNode(dataDomain);
                        } else {
                            this.notifyStatus("Error: failed to remove the " + ConfigWindow.SERVICE_TEXT + " ", false, false);
                        }
                        dataNode.getOwner().removeNode(dataNode);
                    }
                } else if ("add".equals(command)) {
                    dataNode = item.getData();
                    metaNode = item.getChildMeta();
                    if (dataNode.getOwner().isServiceElement(dataNode)
                            && ((MetaElement) metaNode).getName().equals("Service")) {
                        HashMap<String, MetaNode> service = new HashMap<String, MetaNode>();
                        for (MetaNode mn : ((MetaElement) dataNode.getOwner().getRootElement().getMetadata())
                                .getChildren()) {
                            if (!((MetaElement) mn).getName().equals("Domain")) {
                                service.put(((MetaElement) mn).getName(), mn);
                            }
                        }
                        String[] serviceNames = service.keySet().toArray(new String[service.keySet().size()]);
                        String name = (String) JOptionPane.showInputDialog(null, "Please select a service to add.",
                                "Service selection", JOptionPane.QUESTION_MESSAGE, null, serviceNames, serviceNames[0]);
                        if (name != null) {
                            addService(dataNode.getOwner().getRootElement(), service.get(name));
                        }
                    } else {
                        if (dataNode instanceof DataElement) {
                            dataNode.getOwner().addNode((DataElement) dataNode, metaNode);
                        } else {
                            this.notifyStatus("Error: Unexpected type of parent found: '" + dataNode.getClass() + "'.",
                                    false, false);
                        }
                    }
                } else if ("addService".equals(command)) {
                    addService(item.getData(), item.getChildMeta());
                } else if("reset".equals(command)){
                    dataNode = item.getData();
                    dataNode.getOwner().setValue((DataValue)dataNode, ((MetaValue)dataNode.getMetadata()).getDefaultValue());
                } else {
                    this.notifyStatus("Warning: Action for command '" + command + "' not implemented.", false, false);
                }
            }
        } catch(DataException de){
            Report.getInstance().writeErrorLog("DataNodePopup actionPerformed" + de.getMessage());
            this.notifyStatus("Error: " + de.getMessage(), false, false);
            StackTraceElement[] elements = de.getStackTrace();
            String error = "";

            for(int i=0; i<elements.length; i++){
                error += elements[i].getFileName() + ", " +
 elements[i].getMethodName() + ", "
                        + elements[i].getLineNumber() + "\n";
                if(i==0){
                    error = elements[i].getFileName() + ", " +
                            elements[i].getMethodName() + ", " + elements[i].getLineNumber();
                }
            }
            Report.getInstance().writeErrorLog("DataNodePopup actionPerformed\n DataException occurred:\n" + error);
        } catch(Exception ex){
            StackTraceElement[] elements = ex.getStackTrace();
            StringBuffer buf = new StringBuffer();

            for(int i=0; i<elements.length; i++){
                buf.append(elements[i].getFileName() + ", " +
 elements[i].getMethodName() + ", "
                        + elements[i].getLineNumber() + "\n");
            }
            String error = buf.toString();
            Report.getInstance().writeErrorLog(
                    "DataNodePopup actionPerformed\n Unhandled exception occurred:\n" + error);
            this.notifyStatus("Error: " + error, false, false);
            ex.printStackTrace();
        }
    }

    @Override
    public void mouseClicked(MouseEvent e) {
        DataNode dn;
        DataNodePopupSupport support;
        Object source = e.getSource();

        synchronized(popupSupport){
            if(popupSupport.contains(source)){
                if((e.getClickCount() == 1)) {
                    if(e.getButton() == MouseEvent.BUTTON3) {
                        support = (DataNodePopupSupport)source;
                        if (support != null) {
                            dn = support.getDataNodeAt(e.getX(), e.getY());
                            support.showPopup(this.getPopup(support, dn), e.getX(), e.getY());
                        }
                    }
                }
            }
        }
    }

    public void addService(DataNode dataNode, MetaNode metaNode) throws DataException {
        if (dataNode instanceof DataElement) {
          /* get the newly created service element */
          dataNode.getOwner().addNode((DataElement) dataNode, metaNode);
        } else {
            this.notifyStatus("Error: Unexpected type of parent found: '" + dataNode.getClass() + "'.", false, false);
        }
    }

    @Override
    public void mouseEntered(MouseEvent e) {}

    @Override
    public void mouseExited(MouseEvent e) {}

    @Override
    public void mousePressed(MouseEvent e) {}

    @Override
    public void mouseReleased(MouseEvent e) {}
}
