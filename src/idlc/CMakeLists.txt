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
# Verify Maven is available
find_package(Maven 3.0 REQUIRED)

file(GLOB_RECURSE IDLC_G_SOURCES LIST_DIRECTORIES true *.g)
file(GLOB_RECURSE IDLC_G4_SOURCES LIST_DIRECTORIES true *.g4)
file(GLOB_RECURSE IDLC_JAVA_SOURCES LIST_DIRECTORIES true *.java)
file(GLOB_RECURSE IDLC_ST_SOURCES LIST_DIRECTORIES true *.st?)

set(IDLC_JAR "${CMAKE_CURRENT_BINARY_DIR}/target/idlc-jar-with-dependencies.jar")
mark_as_advanced(IDLC_JAR)

# Maven is executed within the idlc directory located in the build directory
# and generated sources are stored in idlc/target. Non-generated sources,
# however, do not reside in the build directory and Maven must be instructed
# to use those. To allow maven to be executed from both the source and build
# directories (idlc may be moved to it's own repository), the pom.xml file is
# pulled through the configure_file function with basedir set to the original
# source directory. It is a cute little hack to avoid having to use different
# profiles etc.
set(basedir "${CMAKE_CURRENT_SOURCE_DIR}/src")
mark_as_advanced(basedir)
set(IDLC_POM_FILE "src/pom.xml.in")
configure_file(${IDLC_POM_FILE} "pom.xml")
configure_file("src/org/eclipse/cyclonedds/Project.java.in" "org/eclipse/cyclonedds/Project.java")

add_custom_command(
    OUTPUT  "${IDLC_JAR}"
    COMMAND "${Maven_EXECUTABLE}"
    ARGS    "-q" "package"
    DEPENDS ${IDLC_POM_FILE} ${IDLC_G_SOURCES} ${IDLC_G4_SOURCES} ${IDLC_JAVA_SOURCES} ${IDLC_ST_SOURCES}
    COMMENT "Building JAR file ${IDLC_JAR}")

include(cmake/IdlcGenerate.cmake)

install(
  FILES "cmake/IdlcGenerate.cmake"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}/idlc"
  COMPONENT dev)

install(
  FILES "${IDLC_SCRIPT_IN}"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}/idlc"
  COMPONENT dev)

install(
  FILES "${IDLC_JAR}"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}/idlc"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  COMPONENT dev)

