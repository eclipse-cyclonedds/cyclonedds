file(READ package.xml package_xml)
string(REPLACE "\t" " " package_xml "${package_xml}")
file(READ CMakeLists.txt cmakelists_txt)
string(REPLACE "\t" " " cmakelists_txt "${cmakelists_txt}")

if(package_xml MATCHES "<version> *([0-9.][0-9.]*) *</version>")
  set(pv "${CMAKE_MATCH_1}")
endif()

if(cmakelists_txt MATCHES "project *\\( *CycloneDDS .*VERSION +([0-9.][0-9.]*)[ \\)].*")
  set(cv "${CMAKE_MATCH_1}")
endif()

message("package.xml version:    ${pv}")
message("CMakeLists.txt version: ${cv}")

if(NOT pv OR NOT cv)
  message(FATAL_ERROR "version extraction failed")
elseif(NOT pv VERSION_EQUAL cv)
  message(FATAL_ERROR "version mismatch")
endif()
