set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/Modules")

include(HashUtilities)

check_hashes(
    _test
    HASH_FILES ${HASH_FILES}
    APPEND_FILES ${APPEND_FILES}
)

if (NOT ${_test})
    message(FATAL_ERROR "Not up to date")
endif()
