set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/Modules")

include(HashUtilities)


append_hashes(
    PREFIX ${PREFIX}
    POSTFIX ${POSTFIX}
    HASH_FILES ${HASH_FILES}
    APPEND_FILES ${APPEND_FILES}
)
