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
package org.eclipse.cyclonedds.common.controller;

/**
 * Represents a result for assignment of a value to a certain field.
 * 
 * @date Nov 25, 2004
 */
public class AssignmentResult{
    private boolean valid;
    private String errorMessage;
    
    /**
     * Constructs a new AssignMentResult.
     * 
     * @param _success true if validation succeeded, false otherwise.
     * @param _errorMessage null when validation succeeded, the failure
     *                      reason otherwise.
     */
    public AssignmentResult(boolean _success, String _errorMessage){
        valid = _success;
        errorMessage = _errorMessage;
    }
    
    /**
     * Provides access to errorMessage.
     * 
     * @return Returns the errorMessage.
     */
    public String getErrorMessage() {
        return errorMessage;
    }
    /**
     * Provides access to valid.
     * 
     * @return Returns the valid.
     */
    public boolean isValid() {
        return valid;
    }
}
