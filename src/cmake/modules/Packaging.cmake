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
if(PACKAGING_INCLUDED)
  return()
endif()
set(PACKAGING_INCLUDED true)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(PACKAGING_MODULE_DIR "${PROJECT_SOURCE_DIR}/cmake/modules/Packaging")
set(CMAKE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}")

# Generates <Package>Config.cmake.
configure_package_config_file(
  "${PACKAGING_MODULE_DIR}/PackageConfig.cmake.in"
  "${CMAKE_PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_CMAKEDIR}")

# Generates <Package>Version.cmake.
write_basic_package_version_file(
  "${CMAKE_PROJECT_NAME}Version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}Version.cmake"
  DESTINATION "${CMAKE_INSTALL_CMAKEDIR}" COMPONENT dev)

if(DDSC_SHARED AND ((NOT DEFINED BUILD_SHARED_LIBS) OR BUILD_SHARED_LIBS))
  # Generates <Package>Targets.cmake file included by <Package>Config.cmake.
  # The files are placed in CMakeFiles/Export in the build tree.
  install(
    EXPORT "${CMAKE_PROJECT_NAME}"
    FILE "${CMAKE_PROJECT_NAME}Targets.cmake"
    NAMESPACE "${CMAKE_PROJECT_NAME}::"
    DESTINATION "${CMAKE_INSTALL_CMAKEDIR}" COMPONENT dev)
endif()


set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION_TWEAK ${PROJECT_VERSION_TWEAK})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})

set(VENDOR_INSTALL_ROOT "ADLINK")
set(CPACK_PACKAGE_NAME ${CMAKE_PROJECT_NAME})
set(CPACK_PACKAGE_VENDOR "ADLINK Technology Inc.")
set(CPACK_PACKAGE_CONTACT "${CMAKE_PROJECT_NAME} core developers <info@adlinktech.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Leading OMG DDS implementation from ADLINK Technology")
set(CPACK_PACKAGE_ICON "${PACKAGING_MODULE_DIR}/vortex.ico")

# WiX requires a .txt file extension for CPACK_RESOURCE_FILE_LICENSE
file(COPY "${PROJECT_SOURCE_DIR}/../LICENSE" DESTINATION "${CMAKE_BINARY_DIR}")
file(RENAME "${CMAKE_BINARY_DIR}/LICENSE" "${CMAKE_BINARY_DIR}/license.txt")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_BINARY_DIR}/license.txt")

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
set(CPACK_COMPONENT_LIB_DISPLAY_NAME "${CMAKE_PROJECT_NAME_FULL} library")
set(CPACK_COMPONENT_LIB_DESCRIPTION  "Library used to run programs with ${CMAKE_PROJECT_NAME_FULL}")
set(CPACK_COMPONENT_DEV_DISPLAY_NAME "${CMAKE_PROJECT_NAME_FULL} development")
set(CPACK_COMPONENT_DEV_DESCRIPTION  "Development files for use with ${CMAKE_PROJECT_NAME_FULL}")

if(WIN32 AND NOT UNIX)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(__arch "win64")
  else()
    set(__arch "win32")
  endif()
  mark_as_advanced(__arch)

  set(CPACK_GENERATOR "WIX;ZIP;${CPACK_GENERATOR}" CACHE STRING "List of package generators")

  set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION}-${__arch}")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "${VENDOR_INSTALL_ROOT}/${CMAKE_PROJECT_NAME_FULL}")

  set(CPACK_WIX_LIGHT_EXTENSIONS "WixUtilExtension")
  set(CPACK_WIX_COMPONENT_INSTALL ON)
  set(CPACK_WIX_ROOT_FEATURE_TITLE "${CMAKE_PROJECT_NAME_FULL}")
  set(CPACK_WIX_PRODUCT_ICON "${PACKAGING_MODULE_DIR}/vortex.ico")
  # Bitmap (.bmp) of size 493x58px
  set(CPACK_WIX_UI_BANNER "${PACKAGING_MODULE_DIR}/banner.bmp")
  # Bitmap (.bmp) of size 493x312px
  set(CPACK_WIX_UI_DIALOG "${PACKAGING_MODULE_DIR}/dialog.png")
  set(CPACK_WIX_PROGRAM_MENU_FOLDER "${CPACK_PACKAGE_NAME_FULL}")
  set(CPACK_WIX_PATCH_FILE "${PACKAGING_MODULE_DIR}/examples.xml")
  set(CPACK_WIX_PROPERTY_ARPHELPLINK "http://www.adlinktech.com/support")
  set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "http://www.adlinktech.com/")
  set(CPACK_WIX_PROPERTY_ARPURLUPDATEINFO "http://www.adlinktech.com/")

  # A constant GUID allows installers to replace existing installations that use the same GUID.
  set(CPACK_WIX_UPGRADE_GUID "1351F59A-972B-4624-A7F1-439381BFA41D")

  include(InstallRequiredSystemLibraries)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
  # CMake prior to v3.6 messes up the name of the packages. >= v3.6 understands CPACK_RPM/DEBIAN_<component>_FILE_NAME
  cmake_minimum_required(VERSION 3.6)

  set(CPACK_COMPONENTS_GROUPING "IGNORE")

  # FIXME: Requiring lsb_release to be installed may be a viable option.

  if(EXISTS "/etc/redhat-release")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(__arch "x86_64")
    else()
      set(__arch "i686")
    endif()

    set(CPACK_GENERATOR "RPM;TGZ;${CPACK_GENERATOR}" CACHE STRING "List of package generators")

    set(CPACK_RPM_COMPONENT_INSTALL ON)
    # FIXME: The package file name must be updated to include the distribution.
    #        See Fedora and Red Hat packaging guidelines for details.
    set(CPACK_RPM_LIB_PACKAGE_NAME "${CMAKE_PROJECT_NAME_DASHED}")
    set(CPACK_RPM_LIB_FILE_NAME "${CPACK_RPM_LIB_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${__arch}.rpm")
    set(CPACK_RPM_DEV_PACKAGE_NAME "${CPACK_RPM_LIB_PACKAGE_NAME}-devel")
    set(CPACK_RPM_DEV_FILE_NAME "${CPACK_RPM_DEV_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${__arch}.rpm")
    set(CPACK_RPM_DEV_PACKAGE_REQUIRES "${CPACK_RPM_LIB_PACKAGE_NAME} = ${CPACK_PACKAGE_VERSION}")
  elseif(EXISTS "/etc/debian_version")
    set(CPACK_DEB_COMPONENT_INSTALL ON)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(__arch "amd64")
    else()
      set(__arch "i386")
    endif()

    set(CPACK_GENERATOR "DEB;TGZ;${CPACK_GENERATOR}" CACHE STRING "List of package generators")

    string(TOLOWER "${CMAKE_PROJECT_NAME_DASHED}" CPACK_DEBIAN_LIB_PACKAGE_NAME)
    set(CPACK_DEBIAN_LIB_FILE_NAME "${CPACK_DEBIAN_LIB_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_${__arch}.deb")
    set(CPACK_DEBIAN_DEV_PACKAGE_DEPENDS "${CPACK_DEBIAN_LIB_PACKAGE_NAME} (= ${CPACK_PACKAGE_VERSION}), libc6 (>= 2.23)")
    set(CPACK_DEBIAN_DEV_PACKAGE_NAME "${CPACK_DEBIAN_LIB_PACKAGE_NAME}-dev")
    set(CPACK_DEBIAN_DEV_FILE_NAME "${CPACK_DEBIAN_DEV_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_${__arch}.deb")
  else()
    # Generic tgz package
    set(CPACK_GENERATOR "TGZ;${CPACK_GENERATOR}" CACHE STRING "List of package generators")
  endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "VxWorks")
  # FIXME: Support for VxWorks packages must still be implemented (probably
  #        just a compressed tarball)
  message(STATUS "Packaging for VxWorks is unsupported")
endif()

# This must always be last!
include(CPack)

