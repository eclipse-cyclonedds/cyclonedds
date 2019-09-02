#
# Copyright(c) 2019 Jeroen Koekkoek <jeroen@koekkoek.nl>
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

if(__LIST_JOIN__)
  return()
endif()
set(__LIST_JOIN__ TRUE)

# list(JOIN ...) was added in CMake 3.12 (3.7 is currently required)
function(LIST_JOIN _list _glue _var)
  set(_str "")
  list(LENGTH ${_list} _len)

  if(_len GREATER 0)
    set(_cnt 0)
    math(EXPR _max ${_len}-1)
    list(GET ${_list} 0 _str)
    if(_len GREATER 1)
      foreach(_cnt RANGE 1 ${_max})
        list(GET ${_list} ${_cnt} _elm)
        set(_str "${_str}${_glue}${_elm}")
      endforeach()
    endif()
  endif()

  set(${_var} "${_str}" PARENT_SCOPE)
endfunction()

