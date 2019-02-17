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

import java.awt.BorderLayout;
import java.awt.Cursor;
import java.awt.Event;
import java.awt.Image;
import java.awt.Toolkit;
import java.awt.event.ActionEvent;
import java.awt.event.KeyEvent;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.io.File;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;

import javax.swing.JFrame;
import javax.swing.JMenu;
import javax.swing.JMenuBar;
import javax.swing.JMenuItem;
import javax.swing.JPanel;
import javax.swing.JTabbedPane;
import javax.swing.KeyStroke;
import javax.swing.SwingUtilities;

import org.eclipse.cyclonedds.common.view.MainWindow;
import org.eclipse.cyclonedds.common.view.StatusPanel;
import org.eclipse.cyclonedds.config.data.DataConfiguration;
import org.eclipse.cyclonedds.config.data.DataConfigurationListener;
import org.eclipse.cyclonedds.config.data.DataElement;
import org.eclipse.cyclonedds.config.data.DataNode;
import org.eclipse.cyclonedds.config.data.DataValue;
import org.eclipse.cyclonedds.config.meta.MetaElement;
import org.eclipse.cyclonedds.config.meta.MetaNode;

public class ConfigWindow extends MainWindow implements DataConfigurationListener {
    private static final long serialVersionUID = 1L;
    private JPanel jContentPane = null;
    private ConfigWindowController controller = null;
    private JMenuBar configMenuBar = null;
    private JTabbedPane mainTabbedPane = null;
    private JMenu fileMenu = null;
    private JMenu editMenu = null;
    private JMenu addServiceMenu = null;
    private JMenu removeServiceMenu = null;
    private JMenuItem newItem = null;
    private JMenuItem openItem = null;
    private JMenuItem saveItem = null;
    private JMenuItem closeItem = null;
    private JMenuItem saveAsItem = null;
    private JMenuItem exitItem = null;
    private JMenuItem printItem = null;
    private JMenu helpMenu = null;
    private JMenuItem helpContentsItem = null;
    private DataConfiguration config = null;
    private List<Image> appLogos = null;


    private DataNodePopup popupSupport = null;
    private ConfigTransferHandler transferHandler = null;

    private String windowTitle = "Eclipse Cyclone DDS Configurator";

    public static String    SERVICE_LABEL = "Component";
    public static String    SERVICE_TEXT = "component";

    /**
     * This is the default constructor
     */
    public ConfigWindow() {
        super();
        initialize();
    }

    public ConfigWindow(String uri) {
        super();
        initialize();
        controller.handleOpenFromUri(uri);
    }

    public DataConfiguration getConfig() {
        return config;
    }

    public void setDataConfiguration(DataConfiguration config){
        DataElement[] services;
        
        if(this.config != null){
            this.config.removeDataConfigurationListener(this);
            int count = mainTabbedPane.getComponentCount();
            
            for(int i=0; i < count; i++){
                mainTabbedPane.remove(0);
            }
        }
        this.config = config;
        
        if(this.config != null){
            this.config.addDataConfigurationListener(this);
            services  = config.getServices();
            
            for(int i=0; i < services.length; i++){
                ServicePanel servicePanel = new ServicePanel(services[i], this.statusPanel);
                servicePanel.setTransferHandler(transferHandler);
                mainTabbedPane.addTab(
                        ConfigUtil.getExtendedDataElementString(services[i]), 
                        servicePanel);
            }
        }
        this.updateMenus();
    }
    
    @Override
    public void nodeAdded(DataElement parent, DataNode nodeAdded) {
        this.updateMenus();
        
        if(parent.equals(this.config.getRootElement())){
            if(nodeAdded instanceof DataElement){
                final DataElement service = (DataElement)nodeAdded;
                
                Runnable worker = new Runnable(){
                    @Override
                    public void run(){
                        ServicePanel servicePanel = new ServicePanel(service, statusPanel);
                        servicePanel.setTransferHandler(transferHandler);
                        mainTabbedPane.addTab(
                                ConfigUtil.getExtendedDataElementString(service), 
                                servicePanel);
                    }
                };
                SwingUtilities.invokeLater(worker);
            }
        }
    }

    @Override
    public void nodeRemoved(DataElement parent, DataNode nodeRemoved) {
        this.updateMenus();
        
        if(parent.equals(this.config.getRootElement())){
            if(nodeRemoved instanceof DataElement){
                final DataElement service = (DataElement)nodeRemoved;
                
                Runnable worker = new Runnable(){
                    @Override
                    public void run(){
                        ServicePanel servicePanel;
                        boolean found = false;
                        
                        for(int i=0; (i<mainTabbedPane.getTabCount()) && (!found); i++){
                            servicePanel = (ServicePanel)mainTabbedPane.getComponentAt(i);
                            
                            if(servicePanel.getService().equals(service)){
                                mainTabbedPane.removeTabAt(i);
                                found = true;
                            }
                        }
                    }
                };
                SwingUtilities.invokeLater(worker);
            }
        }
    }

    @Override
    public void valueChanged(DataValue data, Object oldValue, Object newValue) {
        ServicePanel servicePanel;
        
        for(int i=0; i<this.mainTabbedPane.getTabCount(); i++){
            servicePanel = (ServicePanel)this.mainTabbedPane.getComponentAt(i);
            this.mainTabbedPane.setTitleAt(i, 
                    ConfigUtil.getExtendedDataElementString(
                            servicePanel.getService()));
        }
        this.updateMenus();
    }
    
    public DataConfiguration getDataConfiguration(){
        return this.config;
    }
    
    @Override
    public void disableView(){
        ServicePanel sp;
        
        this.setEnabled(false);
        this.setFocusable(false);
        
        for(int i=0; i<this.mainTabbedPane.getComponentCount(); i++){
            sp = (ServicePanel)this.mainTabbedPane.getComponentAt(i);
            sp.getConfigurationTable().getEditor().stopCellEditing();
        }
    }
    
    public void setActionsEnabled(boolean enabled){
        ServicePanel sp;
        if(enabled){
            this.setCursor(new Cursor(Cursor.DEFAULT_CURSOR));
        } else {
            this.setCursor(new Cursor(Cursor.WAIT_CURSOR));
        }
        for(int i=0; i<this.mainTabbedPane.getComponentCount(); i++){
            sp = (ServicePanel)this.mainTabbedPane.getComponentAt(i);
            sp.setEnabled(enabled);
        }
        this.updateMenus();
    }
    
    /**
     * Enables the view component.
     */
    @Override
    public void enableView(){
        this.setFocusable(true);
        this.setEnabled(true);
        this.toFront();
    }
    
    public ConfigWindowController getController(){
        return this.controller;
    }
    
    public void setWindowTitle () {
        super.setTitle(this.windowTitle);
    }

    private void updateMenus(){
        this.getAddServiceMenu().removeAll();
        this.getRemoveServiceMenu().removeAll();
        
        if(this.config != null){
            if(this.config.isUpToDate()){
                this.getSaveItem().setEnabled(false);
            } else {
                this.getSaveItem().setEnabled(true);
            }
            this.getSaveAsItem().setEnabled(true);
            this.getPrintItem().setEnabled(true);
            this.getCloseItem().setEnabled(true);
            this.getAddServiceMenu().setEnabled(true);
            this.getRemoveServiceMenu().setEnabled(true);
            
            MetaNode[] mn = ((MetaElement)config.getRootElement().getMetadata()).getChildren();
            
            for(MetaNode m: mn){
                if(m instanceof MetaElement){
                    this.getAddServiceMenu().add(this.getAddServiceItem((MetaElement)m));
                }
            }
            for(DataElement e: config.getServices()){
                this.getRemoveServiceMenu().add(this.getRemoveServiceItem(e));
            }
        } else {
            this.getSaveItem().setEnabled(false);
            this.getSaveAsItem().setEnabled(false);
            this.getPrintItem().setEnabled(false);
            this.getCloseItem().setEnabled(false);
            this.getAddServiceMenu().setEnabled(false);
            this.getRemoveServiceMenu().setEnabled(false);
        }
        this.updateTitle();
    }
    
    private void updateTitle(){
        File f;
        String title;
        if(this.config != null){
            f = config.getFile();
            
            if(f != null){
                title = this.windowTitle + " | " + f.getAbsolutePath();
            } else {
                title = this.windowTitle + " | <NoName>";
            }
        } else {
            title = this.windowTitle;
        }
        this.setTitle(title);
    }
    
    private DataNodeMenuItem getAddServiceItem(MetaElement service){
        int curOcc = 0;
        DataNodeMenuItem item = new DataNodeMenuItem(
                                        service.getName(),
                                        this.config.getRootElement(), service);
        
        for(DataElement s: config.getServices()){
            if(s.getMetadata().equals(service)){
                curOcc++;
            }
        }
        if(curOcc < service.getMaxOccurrences()){
            item.setEnabled(true);
        } else {
            item.setEnabled(false);
        }
        item.setActionCommand("addService");
        item.addActionListener(this.popupSupport);
        return item;
    }
    
    private DataNodeMenuItem getRemoveServiceItem(DataElement service){
        int curOcc = 0;
        MetaElement metaService = (MetaElement)service.getMetadata();
        DataNodeMenuItem item = new DataNodeMenuItem(
                            ConfigUtil.getExtendedDataElementString(service),
                            service, null);
        
        for(DataElement s: config.getServices()){
            if(s.getMetadata().equals(metaService)){
                curOcc++;
            }
        }
        if(curOcc > metaService.getMinOccurrences()){
            item.setEnabled(true);
        } else {
            item.setEnabled(false);
        }
        item.setActionCommand("removeService");
        item.addActionListener(this.popupSupport); 
        return item;
    }
    
    /**
     * This method initializes this
     * 
     * @return void
     */
    private void initialize() {
        this.setSize(800, 600);
        this.controller = new ConfigWindowController(this);
        this.popupSupport = new DataNodePopup();
        this.transferHandler = new ConfigTransferHandler(this);
        this.setDefaultCloseOperation(JFrame.DO_NOTHING_ON_CLOSE);
        this.setJMenuBar(getConfigMenuBar());
        this.setLocationRelativeTo(this.getParent());
        this.setContentPane(getJContentPane());
        
        this.addWindowListener(new WindowAdapter() {
            @Override
            public void windowClosing(WindowEvent e) {
                controller.actionPerformed(
                    new ActionEvent(exitItem, 0, "exit"));
            }
        });
        this.updateMenus();
        this.setAppLogo();
    }

    /**
     * This method initializes jContentPane
     * 
     * @return javax.swing.JPanel
     */
    private JPanel getJContentPane() {
        if (jContentPane == null) {
            jContentPane = new JPanel();
            jContentPane.setLayout(new BorderLayout());
            jContentPane.add(getMainTabbedPane(), BorderLayout.CENTER);
            jContentPane.add(getStatusPanel(), BorderLayout.SOUTH);
        }
        return jContentPane;
    }

    /**
     * This method initializes statusPanel.  
     *  
     * @return The status panel of the window.
     */    
    @Override
    protected StatusPanel getStatusPanel() {
        if (statusPanel == null) {
            statusPanel = new StatusPanel(300, "Ready", false, true);
        }
        return statusPanel;
    }
    
    /**
     * This method initializes configMenuBar	
     * 	
     * @return javax.swing.JMenuBar	
     */
    private JMenuBar getConfigMenuBar() {
        if (configMenuBar == null) {
            configMenuBar = new JMenuBar();
            configMenuBar.add(getFileMenu());
            configMenuBar.add(getEditMenu());
            configMenuBar.add(getHelpMenu());
        }
        return configMenuBar;
    }

    /**
     * This method initializes mainTabbedPane	
     * 	
     * @return javax.swing.JTabbedPane	
     */
    private JTabbedPane getMainTabbedPane() {
        if (mainTabbedPane == null) {
            mainTabbedPane = new JTabbedPane();
            mainTabbedPane.setTransferHandler(this.transferHandler);
        }
        return mainTabbedPane;
    }

    /**
     * This method initializes fileMenu	
     * 	
     * @return javax.swing.JMenu	
     */
    private JMenu getFileMenu() {
        if (fileMenu == null) {
            fileMenu = new JMenu();
            fileMenu.setText("File");
            fileMenu.setMnemonic(KeyEvent.VK_F);
            fileMenu.add(getNewItem());
            fileMenu.add(getOpenItem());
            fileMenu.add(getCloseItem());
            fileMenu.addSeparator();
            fileMenu.add(getSaveItem());
            fileMenu.add(getSaveAsItem());
            fileMenu.addSeparator();
            fileMenu.add(getPrintItem());
            fileMenu.addSeparator();
            fileMenu.add(getExitItem());
        }
        return fileMenu;
    }
    
    private JMenu getEditMenu(){
        if (editMenu == null){
            editMenu = new JMenu();
            editMenu.setText("Edit");
            editMenu.setMnemonic(KeyEvent.VK_E);
            editMenu.add(getAddServiceMenu());
            editMenu.add(getRemoveServiceMenu());
        }
        return editMenu;
    }
    
    private JMenu getHelpMenu(){
        if (helpMenu == null){
            helpMenu = new JMenu();
            helpMenu.setText("Help");
            helpMenu.setMnemonic(KeyEvent.VK_H);
            helpMenu.add(getHelpContentsItem());
        }
        return helpMenu;
    }
    
    private JMenuItem getHelpContentsItem(){
        if (helpContentsItem == null){
            helpContentsItem = new JMenuItem();
            helpContentsItem.setText("Help Contents");
            helpContentsItem.setMnemonic(KeyEvent.VK_C);
            helpContentsItem.setActionCommand("help");
            helpContentsItem.addActionListener(getController());
            helpContentsItem.setAccelerator(KeyStroke.getKeyStroke("control H"));
        }
        return helpContentsItem;
    }
    
    private JMenu getAddServiceMenu(){
        if (addServiceMenu == null){
            addServiceMenu = new JMenu();
            addServiceMenu.setText("Add " + SERVICE_LABEL);
            addServiceMenu.setMnemonic(KeyEvent.VK_A);
        }
        return addServiceMenu;
    }
    
    private JMenu getRemoveServiceMenu(){
        if (removeServiceMenu == null){
            removeServiceMenu = new JMenu();
            removeServiceMenu.setText("Remove " + SERVICE_LABEL);
            removeServiceMenu.setMnemonic(KeyEvent.VK_R);
        }
        return removeServiceMenu;
    }

    /**
     * This method initializes newItem	
     * 	
     * @return javax.swing.JMenuItem	
     */
    private JMenuItem getNewItem() {
        if (newItem == null) {
            newItem = new JMenuItem();
            newItem.setText("New...");
            newItem.setMnemonic(KeyEvent.VK_N);
            newItem.setAccelerator(KeyStroke.getKeyStroke("control N"));
            newItem.setActionCommand("new");
            newItem.addActionListener(controller);
        }
        return newItem;
    }

    /**
     * This method initializes openItem	
     * 	
     * @return javax.swing.JMenuItem	
     */
    private JMenuItem getOpenItem() {
        if (openItem == null) {
            openItem = new JMenuItem();
            openItem.setText("Open...");
            openItem.setMnemonic(KeyEvent.VK_O);
            openItem.setAccelerator(KeyStroke.getKeyStroke("control O"));
            openItem.setActionCommand("open");
            openItem.addActionListener(controller);
        }
        return openItem;
    }

    /**
     * This method initializes saveItem	
     * 	
     * @return javax.swing.JMenuItem	
     */
    private JMenuItem getSaveItem() {
        if (saveItem == null) {
            saveItem = new JMenuItem();
            saveItem.setText("Save");
            saveItem.setMnemonic(KeyEvent.VK_S);
            saveItem.setAccelerator(KeyStroke.getKeyStroke("control S"));
            saveItem.setActionCommand("save");
            saveItem.addActionListener(controller);
        }
        return saveItem;
    }

    /**
     * This method initializes closeItem	
     * 	
     * @return javax.swing.JMenuItem	
     */
    private JMenuItem getCloseItem() {
        if (closeItem == null) {
            closeItem = new JMenuItem();
            closeItem.setText("Close");
            closeItem.setMnemonic(KeyEvent.VK_C);
            closeItem.setActionCommand("close");
            closeItem.addActionListener(controller);
        }
        return closeItem;
    }

    /**
     * This method initializes saveAsItem	
     * 	
     * @return javax.swing.JMenuItem	
     */
    private JMenuItem getSaveAsItem() {
        if (saveAsItem == null) {
            saveAsItem = new JMenuItem();
            saveAsItem.setText("Save As...");
            saveAsItem.setMnemonic(KeyEvent.VK_A);
            saveAsItem.setActionCommand("save_as");
            saveAsItem.addActionListener(controller);
        }
        return saveAsItem;
    }

    /**
     * This method initializes exitItem	
     * 	
     * @return javax.swing.JMenuItem	
     */
    private JMenuItem getExitItem() {
        if (exitItem == null) {
            exitItem = new JMenuItem();
            exitItem.setText("Exit");
            exitItem.setMnemonic(KeyEvent.VK_X);
            exitItem.setAccelerator(KeyStroke.getKeyStroke("alt F4"));
            exitItem.setActionCommand("exit");
            exitItem.addActionListener(controller);
        }
        return exitItem;
    }

    /**
     * This method initializes printItem	
     * 	
     * @return javax.swing.JMenuItem	
     */
    private JMenuItem getPrintItem() {
        if (printItem == null) {
            printItem = new JMenuItem();
            printItem.setText("Print...");
            printItem.setMnemonic(KeyEvent.VK_P);
            printItem.setAccelerator(KeyStroke.getKeyStroke("control P"));
            printItem.setActionCommand("print");
            printItem.addActionListener(controller);
        }
        return printItem;
    }

    /**
     * Set the Logo image to be used in Configurator's window and taskbar button.
     */
    private void setAppLogo() {
        if (appLogos == null) {
            try {
                List<URL> imgUrls = new ArrayList<URL>(4);
                // Expected location of the icons in tuner jar
                URL url = getClass().getResource("/resources/ptlogoc16.png");
                imgUrls.add(url != null ? url : getClass().getResource("/ptlogoc16.png"));
                url = getClass().getResource("/resources/ptlogoc24.png");
                imgUrls.add(url != null ? url : getClass().getResource("/ptlogoc24.png"));
                url = getClass().getResource("/resources/ptlogoc32.png");
                imgUrls.add(url != null ? url : getClass().getResource("/ptlogoc32.png"));
                url = getClass().getResource("/resources/ptlogoc48.png");
                imgUrls.add(url != null ? url : getClass().getResource("/ptlogoc48.png"));

                appLogos = new ArrayList<Image>(4);
                for (URL imgUrl : imgUrls) {
                    if (imgUrl != null) {
                        appLogos.add(Toolkit.getDefaultToolkit().getImage(imgUrl));
                    }
                }
                this.setIconImages(appLogos);
            } catch (Exception e) {}
        }
    }
}
