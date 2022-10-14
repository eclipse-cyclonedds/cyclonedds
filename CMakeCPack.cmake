#
# Copyright(c) 2020 to 2022 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
if(CMAKECPACK_INCLUDED)
  return()
endif()
set(CMAKECPACK_INCLUDED true)

include(GNUInstallDirs)
set(PROJECT_NAME_FULL "Eclipse Cyclone DDS")
# Set some convenience variants of the project-name
string(REPLACE " " "-" PROJECT_NAME_DASHED "${PROJECT_NAME_FULL}")

set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION_TWEAK ${PROJECT_VERSION_TWEAK})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VENDOR "Eclipse Cyclone DDS project")
set(CPACK_PACKAGE_CONTACT "https://github.com/eclipse-cyclonedds/cyclonedds")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Eclipse Cyclone DDS")

file(COPY "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION "${CMAKE_BINARY_DIR}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_BINARY_DIR}/LICENSE")

# Packages could be generated on alien systems. e.g. Debian packages could be
# created on Red Hat Enterprise Linux, but since packages also need to be
# verified on the target platform, please refrain from doing so. Another
# reason for building installer packages on the target platform is to ensure
# the binaries are linked to the libc version shipped with that platform. To
# support "generic" Linux distributions, eventually compressed tarballs will
# be shipped.
#
# NOTE: Settings for different platforms are in separate control branches.
#       Although that does not make sense from a technical point-of-view, it
#       does help to clearify which settings are required for a platform.

set(CPACK_COMPONENTS_ALL dev lib)
set(CPACK_COMPONENT_LIB_DISPLAY_NAME "${PROJECT_NAME_FULL} library")
set(CPACK_COMPONENT_LIB_DESCRIPTION  "Library used to run programs with ${PROJECT_NAME_FULL}")
set(CPACK_COMPONENT_DEV_DISPLAY_NAME "${PROJECT_NAME_FULL} development")
set(CPACK_COMPONENT_DEV_DESCRIPTION  "Development files for use with ${PROJECT_NAME_FULL}")

if(WIN32 AND NOT UNIX)
  file(COPY "${PROJECT_SOURCE_DIR}/WiX/LICENSE.rtf" DESTINATION "${CMAKE_BINARY_DIR}")
  set(CPACK_WIX_LICENSE_RTF "${CMAKE_BINARY_DIR}/LICENSE.rtf")
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(__arch "win64")
  else()
    set(__arch "win32")
  endif()
  mark_as_advanced(__arch)

  set(CPACK_GENERATOR "WIX;ZIP;${CPACK_GENERATOR}" CACHE STRING "List of package generators")
  set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${CPACK_PACKAGE_VERSION}-${__arch}")
  set(WIX_DIR "${PROJECT_SOURCE_DIR}/WiX")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME_FULL}")
  set(CPACK_WIX_UI_REF "CustomUI_InstallDir")
  set(CPACK_WIX_PATCH_FILE "${WIX_DIR}/env.xml")
  set(CPACK_WIX_EXTRA_SOURCES "${WIX_DIR}/PathDlg.wxs" 
                              "${WIX_DIR}/DialogOrder.wxs")
  set(CPACK_WIX_CMAKE_PACKAGE_REGISTRY "${PROJECT_NAME}")
  set(CPACK_WIX_PRODUCT_ICON "${WIX_DIR}/icon.ico")
  set(CPACK_WIX_UI_BANNER "${WIX_DIR}/banner.png")
  set(CPACK_WIX_UI_DIALOG "${WIX_DIR}/dialog.png")
  # when updating the version number also generate a new GUID
	set(CPACK_WIX_UPGRADE_GUID "f619c294-0696-4f04-98ed-4cfa6ebba6a5")

  include(InstallRequiredSystemLibraries)
  set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)

elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(CPACK_COMPONENTS_GROUPING "IGNORE")

  if(EXISTS "/etc/redhat-release")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(__arch "x86_64")
    else()
      set(__arch "i686")
    endif()

    set(CPACK_GENERATOR "RPM;TGZ;${CPACK_GENERATOR}" CACHE STRING "List of package generators")
    set(CPACK_RPM_PACKAGE_LICENSE "Eclipse Public License v2.0  http://www.eclipse.org/legal/epl-2.0")
    set(CPACK_RPM_COMPONENT_INSTALL ON)
    set(CPACK_RPM_PACKAGE_RELEASE 1)
    set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
    set(CPACK_RPM_LIB_PACKAGE_NAME "${PROJECT_NAME_DASHED}")
    set(CPACK_RPM_LIB_FILE_NAME "${CPACK_RPM_LIB_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}%{?dist}-${__arch}.rpm")
    set(CPACK_RPM_DEV_PACKAGE_NAME "${CPACK_RPM_LIB_PACKAGE_NAME}-devel")
    set(CPACK_RPM_DEV_FILE_NAME "${CPACK_RPM_DEV_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}%{?dist}-${__arch}.rpm")
    set(CPACK_RPM_DEV_PACKAGE_REQUIRES "${CPACK_RPM_LIB_PACKAGE_NAME} = ${CPACK_PACKAGE_VERSION}")
  elseif(EXISTS "/etc/debian_version")
    set(CPACK_DEB_COMPONENT_INSTALL ON)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(__arch "amd64")
    else()
      set(__arch "i386")
    endif()

    set(CPACK_GENERATOR "DEB;TGZ;${CPACK_GENERATOR}" CACHE STRING "List of package generators")

    string(TOLOWER "${PROJECT_NAME_DASHED}" CPACK_DEBIAN_LIB_PACKAGE_NAME)
    set(CPACK_DEBIAN_LIB_FILE_NAME "${CPACK_DEBIAN_LIB_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_${__arch}.deb")
    set(CPACK_DEBIAN_DEV_PACKAGE_DEPENDS "${CPACK_DEBIAN_LIB_PACKAGE_NAME} (= ${CPACK_PACKAGE_VERSION}), libc6 (>= 2.23)")
    set(CPACK_DEBIAN_DEV_PACKAGE_NAME "${CPACK_DEBIAN_LIB_PACKAGE_NAME}-dev")
    set(CPACK_DEBIAN_DEV_FILE_NAME "${CPACK_DEBIAN_DEV_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_${__arch}.deb")
  else()
    # Generic tgz package
    set(CPACK_GENERATOR "TGZ;${CPACK_GENERATOR}" CACHE STRING "List of package generators")
  endif()
else()
    # Fallback to zip package
    set(CPACK_GENERATOR "ZIP;${CPACK_GENERATOR}" CACHE STRING "List of package generators")
endif()

# This must always be last!
include(CPack)
