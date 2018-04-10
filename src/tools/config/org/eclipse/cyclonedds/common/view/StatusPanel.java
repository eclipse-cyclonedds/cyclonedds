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

import java.awt.Color;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;

import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JProgressBar;
import javax.swing.SwingUtilities;
import javax.swing.Timer;

import org.eclipse.cyclonedds.common.util.Config;

/**
 * This class provides a standard statusbar that can be placed in a window at
 * any place.
 */
public class StatusPanel extends JPanel {

    private static final long serialVersionUID = -6861447244439379176L;

    /**
     * Creates a new StatusPanel that can be used to provide information about
     * the status of the application of a certain action. The StatusPanel
     * optionally provides a connection light and a progressbar.
     * 
     * @param width
     *            The width of the panel.
     * @param _defaultText
     *            The default text of the panel.
     * @param showConnectionLight
     *            Whether or not to display a connection light.
     * @param showProgressBar
     *            Whether or not to display a progressbar.
     */
    public StatusPanel(int width, String _defaultText,
            boolean showConnectionLight, boolean showProgressBar) {
        super();
        this.setLayout(new java.awt.BorderLayout());
        defaultText = _defaultText;

        if (defaultText == null) {
            defaultText = "";
        }
        defaultBg = this.getBackground();
        status = new JLabel();
        status.setText(defaultText);
        status.setToolTipText(defaultText);
        java.awt.GridLayout layGridLayout2 = new java.awt.GridLayout();
        layGridLayout2.setRows(1);
        layGridLayout2.setColumns(1);

        if (showConnectionLight && showProgressBar) {
            progressBar = new JProgressBar(0, 10);
            progressBar.setBorderPainted(false);
            progressBar.setPreferredSize(new java.awt.Dimension(
                    width / 10 + 10, 10));

            JPanel temp = new JPanel();
            temp.setLayout(new java.awt.BorderLayout());
            temp.setBorder(javax.swing.BorderFactory
                    .createBevelBorder(javax.swing.border.BevelBorder.LOWERED));

            statusPanel = new JPanel();
            statusPanel.setLayout(layGridLayout2);
            statusPanel.setPreferredSize(new java.awt.Dimension(width - 20
                    - (width / 10) - 10, 20));
            statusPanel.setMinimumSize(new java.awt.Dimension(50, 20));
            statusPanel.add(status);

            temp.add(statusPanel, java.awt.BorderLayout.CENTER);
            temp.add(progressBar, java.awt.BorderLayout.EAST);

            connectionPanel = new JPanel();
            connectionPanel.setPreferredSize(new java.awt.Dimension(20, 10));
            connectionPanel
                    .setBorder(javax.swing.BorderFactory.createCompoundBorder(
                            javax.swing.BorderFactory
                                    .createCompoundBorder(
                                            javax.swing.BorderFactory
                                                    .createLineBorder(
                                                            connectionPanel
                                                                    .getBackground(),
                                                            2),
                                            javax.swing.BorderFactory
                                                    .createEtchedBorder(javax.swing.border.EtchedBorder.RAISED)),
                            javax.swing.BorderFactory
                                    .createBevelBorder(javax.swing.border.BevelBorder.LOWERED)));
            connectionPanel.setBackground(disconnectedColor);
            connectionPanel.setToolTipText("Not connected.");

            this.add(temp, java.awt.BorderLayout.CENTER);
            this.add(connectionPanel, java.awt.BorderLayout.EAST);
            this.setPreferredSize(new java.awt.Dimension(width, 20));
        } else if (showProgressBar) {
            progressBar = new JProgressBar(0, 10);
            progressBar.setBorderPainted(false);
            progressBar.setPreferredSize(new java.awt.Dimension(
                    width / 10 + 10, 10));

            statusPanel = new JPanel();
            statusPanel.setLayout(layGridLayout2);
            statusPanel.setPreferredSize(new java.awt.Dimension(width
                    - (width / 10) - 10, 20));
            statusPanel.setMinimumSize(new java.awt.Dimension(50, 20));
            statusPanel.add(status);

            this.setBorder(javax.swing.BorderFactory
                    .createBevelBorder(javax.swing.border.BevelBorder.LOWERED));
            this.setPreferredSize(new java.awt.Dimension(width, 20));
            this.add(statusPanel, java.awt.BorderLayout.CENTER);
            this.add(progressBar, java.awt.BorderLayout.EAST);
        } else if (showConnectionLight) {
            statusPanel = new JPanel();
            statusPanel.setLayout(layGridLayout2);
            statusPanel.setBorder(javax.swing.BorderFactory
                    .createBevelBorder(javax.swing.border.BevelBorder.LOWERED));
            statusPanel
                    .setPreferredSize(new java.awt.Dimension(width - 20, 20));
            statusPanel.setMinimumSize(new java.awt.Dimension(50, 20));
            statusPanel.add(status);

            connectionPanel = new JPanel();
            connectionPanel.setPreferredSize(new java.awt.Dimension(20, 10));
            connectionPanel
                    .setBorder(javax.swing.BorderFactory.createCompoundBorder(
                            javax.swing.BorderFactory
                                    .createCompoundBorder(
                                            javax.swing.BorderFactory
                                                    .createLineBorder(
                                                            connectionPanel
                                                                    .getBackground(),
                                                            2),
                                            javax.swing.BorderFactory
                                                    .createEtchedBorder(javax.swing.border.EtchedBorder.RAISED)),
                            javax.swing.BorderFactory
                                    .createBevelBorder(javax.swing.border.BevelBorder.LOWERED)));
            connectionPanel.setBackground(disconnectedColor);
            connectionPanel.setToolTipText("Not connected.");

            this.add(statusPanel, java.awt.BorderLayout.CENTER);
            this.add(connectionPanel, java.awt.BorderLayout.EAST);
            this.setPreferredSize(new java.awt.Dimension(width, 20));
        } else {
            this.setLayout(layGridLayout2);
            this.setBorder(javax.swing.BorderFactory
                    .createBevelBorder(javax.swing.border.BevelBorder.LOWERED));
            this.setPreferredSize(new java.awt.Dimension(width, 20));
            this.setMinimumSize(new java.awt.Dimension(100, 20));
            this.add(status);
        }
    }

    /**
     * Sets the connection light to green when connected and to red if not.
     * 
     * @param connected
     *            Whether or not the application has a connection.
     * @param toolTip
     *            The tooltip that must be displayed when the user moves the
     *            mouse over the connection light.
     */
    public synchronized void setConnected(boolean connected, String toolTip) {
        if (connectionPanel != null) {
            final boolean con = connected;
            final String tt = toolTip;

            Runnable runner = new Runnable() {
                @Override
                public void run() {
                    if (con) {
                        connectionPanel.setBackground(connectedColor);
                    } else {
                        connectionPanel.setBackground(disconnectedColor);
                    }
                    connectionPanel.setToolTipText(tt);
                }
            };
            SwingUtilities.invokeLater(runner);
        }
    }

    /**
     * Sets the status message to the supplied message and sets the progressbar
     * to indeterminate mode or resets it. This is done by calling the
     * setStatus(String, boolean) method as well as the setBusy method.
     * 
     * @param message
     *            The message to set.
     * @param persistent
     *            Whether or not the message should never be removed or not.
     * @param busy
     *            Whether or not the progressbar must be set in indeterminate
     *            mode or not.
     */
    public synchronized void setStatus(String msg, boolean persistent,
            boolean busy) {
        if (msg == null) {
            msg = "";
        }
        final String message = msg;
        final boolean b = busy;
        final boolean p = persistent;

        /*
         * Runnable runner = new Runnable() { public void run(){
         */
        String myMsg = message;

        if ("".equals(message)) {
            myMsg = defaultText;
        }
        if (message.startsWith("Error:")) {
            if (statusPanel != null) {
                statusPanel.setBackground(errorColor);

                if (progressBar != null) {
                    progressBar.setBackground(errorColor);
                }
            } else {
                setBackground(errorColor);
            }
            myMsg = message.substring(6);
        } else if (message.startsWith("Warning:")) {
            if (statusPanel != null) {
                statusPanel.setBackground(warningColor);

                if (progressBar != null) {
                    progressBar.setBackground(warningColor);
                }
            } else {
                setBackground(warningColor);
            }
            myMsg = message.substring(8);
        } else { // Standard message
            if (statusPanel != null) {
                statusPanel.setBackground(defaultBg);

                if (progressBar != null) {
                    progressBar.setBackground(defaultBg);
                }
            } else {
                setBackground(defaultBg);
            }
        }
        status.setText(" " + myMsg);
        status.setToolTipText(myMsg);
        setBusy(b);

        synchronized (this) {
            if (!p) {
                Timer persistentTimer = new Timer(persistentTime,
                        new ActionListener() {
                            @Override
                            public void actionPerformed(ActionEvent e) {
                                long currentTime = System.currentTimeMillis();
                                synchronized (org.eclipse.cyclonedds.common.view.StatusPanel.this) {
                                    if (((currentTime - lastMessageTime) < persistentTime)
                                            || (!lastPersistent)) {
                                        /*
                                         * New message has arrived, ignore
                                         * reset.
                                         */
                                    } else {
                                        setStatus(defaultText, false, false);
                                    }
                                }
                            }
                        });
                persistentTimer.setRepeats(false);
                lastMessageTime = System.currentTimeMillis();
                persistentTimer.start();
                lastPersistent = true;
            } else {
                lastPersistent = false;
            }
        }
        /*
         * }
         * 
         * }; runner.run(); SwingUtilities.invokeLater(runner);
         */
    }

    /**
     * Changes the status message. If the message starts with "Error:" the
     * background color will become red and the "Error:" part of the string will
     * not be shown. If the message starts with "Warning:" the background color
     * will become yellow and the "Warning:" part of the string will not be
     * displayed.
     * 
     * @param message
     *            The new status message.
     * @param persistent
     *            If true, the message will not be changed into the standard
     *            text after a few seconds, but stay there until this function
     *            is called again.
     */
    public synchronized void setStatus(String message, boolean persistent) {
        this.setStatus(message, persistent, false);
    }

    /**
     * Sets the progressbar status. When true, the progressbar will go into
     * inderminate mode and display 'busy...', when false the progressbar will
     * leave indeterminate mode and display 'ready'
     * 
     * @param busy
     *            When true, the progressbar will go into inderminate mode
     */
    private synchronized void setBusy(boolean busy) {
        if (progressBar != null) {
            if (busy) {
                progressBar.setBorderPainted(true);
                progressBar.setStringPainted(true);
                progressBar.setString("busy..");
            } else {
                progressBar.setBorderPainted(false);
                progressBar.setStringPainted(false);
                progressBar.setString(null);
            }
            progressBar.setIndeterminate(busy);
        }
    }

    /**
     * Provides access to the current status.
     * 
     * @return The currently displayed status.
     */
    public synchronized String getStatus() {
        return status.getText();
    }

    private static final int persistentTime = 4000;

    private long lastMessageTime;

    private boolean lastPersistent = false;

    /**
     * The current status message.
     */
    private JLabel status = null;

    /**
     * The default message.
     */
    private String defaultText = null;

    /**
     * The default background color
     */
    private Color defaultBg = null;

    /**
     * Panel that holds the connection light. Green if connected, red if not
     * connected.
     */
    private JPanel connectionPanel = null;

    /**
     * Progressbar that can be used to provide the user with progress
     * information.
     */
    private JProgressBar progressBar = null;

    /**
     * Panel that holds the status label.
     */
    private JPanel statusPanel = null;

    private final Color connectedColor = Config.getActiveColor();

    private final Color errorColor = Config.getErrorColor();

    private final Color warningColor = Config.getWarningColor();

    private final Color disconnectedColor = Config.getErrorColor();
}
