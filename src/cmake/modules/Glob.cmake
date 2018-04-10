#
# Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
function(glob variable extension)
  set(dirname "${CMAKE_CURRENT_SOURCE_DIR}")

  foreach(filename ${ARGN})
    unset(filenames)

    if((NOT IS_ABSOLUTE "${filename}") AND
       (EXISTS "${dirname}/${filename}"))
      set(filename "${dirname}/${filename}")
    endif()

    if(IS_DIRECTORY "${filename}")
      file(GLOB_RECURSE filenames "${filename}/*.${extension}")
    elseif(EXISTS "${filename}")
      if("${filename}" MATCHES "\.${extension}$")
        set(filenames "${filename}")
      endif()
    else()
      message(FATAL_ERROR "File ${filename} does not exist")
    endif()

    list(APPEND files ${filenames})
  endforeach()

  set(${variable} "${files}" PARENT_SCOPE)
endfunction()

