if(NOT DEFINED PROTOBUF_SOURCE_DIR)
  message(FATAL_ERROR "PROTOBUF_SOURCE_DIR is not set")
endif()

set(abseil_copts "${PROTOBUF_SOURCE_DIR}/third_party/abseil-cpp/absl/copts/AbseilConfigureCopts.cmake")
if(NOT EXISTS "${abseil_copts}")
  message(FATAL_ERROR "Cannot find ${abseil_copts}")
endif()

file(READ "${abseil_copts}" contents)
string(REPLACE
  [=[list(APPEND ABSL_RANDOM_RANDEN_COPTS "-Xarch_${_arch}" "${_flag}")]=]
  [=[list(APPEND ABSL_RANDOM_RANDEN_COPTS "SHELL:-Xarch_${_arch} ${_flag}")]=]
  contents "${contents}")
file(WRITE "${abseil_copts}" "${contents}")
