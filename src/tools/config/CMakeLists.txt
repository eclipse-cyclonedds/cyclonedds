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
find_package(Java 1.8 REQUIRED)
include(UseJava)
set(CONFJAR_TARGET "${CMAKE_PROJECT_NAME_SMALL}conf")

#set(CMAKE_JAVA_COMPILE_FLAGS "-source" "1.8" "-target" "1.8" -Xlint:deprecation)

# Oddly enough, add_jar can't deal with a resource having an absolute path (experienced on Windows), so
# generate into the list-dir.
configure_file("metaconfig.xml.in" "${CMAKE_CURRENT_LIST_DIR}/metaconfig.xml" @ONLY)

file(GLOB_RECURSE JAVA_SOURCES LIST_DIRECTORIES true *.java)

set(RESOURCES metaconfig.xml metaconfig.xsd resources/ptlogoc16.png resources/ptlogoc24.png resources/ptlogoc32.png resources/ptlogoc48.png)
add_jar(${CONFJAR_TARGET} ${JAVA_SOURCES} ${RESOURCES} ENTRY_POINT org.eclipse.cyclonedds.config.SpliceConfig)

#add_test(NAME TestHelloWorld COMMAND ${Java_JAVA_EXECUTABLE} -cp ${_jarFile} HelloWorld)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/${CONFJAR_TARGET}.jar"
  DESTINATION "${CMAKE_INSTALL_TOOLSDIR}"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  COMPONENT dev)
