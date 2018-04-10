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
package org.eclipse.cyclonedds.common.view;

import javax.swing.JFrame;
import javax.swing.ToolTipManager;

/**  
 * Abstract class that is typically extended from by main windows of an
 * application. It offers a StatusPanel, which is capable of displaying 
 * the current status and implements the ModelListener interface to be able
 * to be attached to ModelRegister components and receive updates from it. This
 * class has been defined abstract because only descendants of this class may
 * exist.
 * 
 * @date Sep 1, 2004
 */
public abstract class MainWindow extends JFrame {
    private static final long serialVersionUID = 5540350112074790669L;
    protected StatusPanel statusPanel    = null;
    
	/**
	 * This is the default constructor
	 */
	public MainWindow() {
		super();
        ToolTipManager tm = ToolTipManager.sharedInstance();
        tm.setInitialDelay(100);
		this.setDefaultCloseOperation(javax.swing.WindowConstants.DO_NOTHING_ON_CLOSE);
        this.setSize(800, 600);
	}
    
    /**
     * This method initializes statusPanel.  
     *  
     * @return The status panel of the window.
     */    
    protected StatusPanel getStatusPanel() {
        if (statusPanel == null) {
            statusPanel = new StatusPanel(300, "Ready", true, true);
        }
        return statusPanel;
    }
    
    /**
     * Sends a message to the statusPanel and sets the progress.
     * 
     * @param message The message to show in the statusbar.
     * @param persistent true if the message must be shown until a new call to
     *                   this function, or false when it should automatically be
     *                   removed after certain amount of time.
     * @param busy Sets the progress monitor of the statusbar.
     */
    public void setStatus(String message, boolean persistent, boolean busy){
        statusPanel.setStatus(message, persistent, busy);
    }
    
    /**
     * Sends a message to the statusPanel.
     * 
     * @param message The message to show in the statusbar.
     * @param persistent true if the message must be shown until a new call to
     *                   this function, or false when it should automatically be
     *                   removed after certain amount of time.
     */
    public void setStatus(String message, boolean persistent){
        statusPanel.setStatus(message, persistent);
    }
    
    /**
     * Provides access to the current status.
     * 
     * @return The currently shown status.
     */
    public String getStatus(){
        return statusPanel.getStatus();
    }
    
    /**
     * Disables the view component. 
     * 
     * This is done when a dialog is shown and the user must
     * provide input before proceeding.
     */
    public void disableView(){
        this.setEnabled(false);
        this.setFocusable(false);
    }
    
    /**
     * Enables the view component.
     */
    public void enableView(){
        this.setFocusable(true);
        this.setEnabled(true);
    }
}
