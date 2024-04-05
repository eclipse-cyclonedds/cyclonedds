// Copyright(c) 2024 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#ifndef DDS_SYSDEF_PARSER_H
#define DDS_SYSDEF_PARSER_H

#include "dds/dds.h"
#include "dds/ddsrt/log.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define SYSDEF_SCOPE_TYPE_LIB        (1u << 0u)
#define SYSDEF_SCOPE_QOS_LIB         (1u << 1u)
#define SYSDEF_SCOPE_DOMAIN_LIB      (1u << 2u)
#define SYSDEF_SCOPE_PARTICIPANT_LIB (1u << 3u)
#define SYSDEF_SCOPE_APPLICATION_LIB (1u << 4u)
#define SYSDEF_SCOPE_NODE_LIB        (1u << 5u)
#define SYSDEF_SCOPE_DEPLOYMENT_LIB  (1u << 6u)

#define SYSDEF_SCOPE_ALL_LIB ( \
  SYSDEF_SCOPE_TYPE_LIB        |\
  SYSDEF_SCOPE_QOS_LIB         |\
  SYSDEF_SCOPE_DOMAIN_LIB      |\
  SYSDEF_SCOPE_PARTICIPANT_LIB |\
  SYSDEF_SCOPE_APPLICATION_LIB |\
  SYSDEF_SCOPE_NODE_LIB        |\
  SYSDEF_SCOPE_DEPLOYMENT_LIB  \
)

#define SYSDEF_RET_OK      0
#define SYSDEF_RET_FAILURE 1

#define SD_PARSE_RESULT_OK 0
#define SD_PARSE_RESULT_ERR -1
#define SD_PARSE_RESULT_SYNTAX_ERR -2
#define SD_PARSE_RESULT_OUT_OF_RESOURCES -3
#define SD_PARSE_RESULT_NOT_SUPPORTED -4
#define SD_PARSE_RESULT_INVALID_REF -5
#define SD_PARSE_RESULT_DUPLICATE -6

/**
 * @defgroup sysdef_parser (SysdefParser)
 */

#define SYSDEF_TRACE(...) DDS_LOG(DDS_LC_SYSDEF | DDS_LC_TRACE, __VA_ARGS__)
#define SYSDEF_ERROR(...) DDS_LOG(DDS_LC_SYSDEF | DDS_LC_ERROR, __VA_ARGS__)

/**
 * @brief Sample structure of System definition.
 * @ingroup sysdef_parser
 * @componen sysdef_parser_api
 */
struct dds_sysdef_system;
/**
 * @brief Sample structure of System definition for data types.
 * @ingroup sysdef_parser
 * @componen sysdef_parser_api
 */
struct dds_sysdef_type_metadata_admin;

/**
 * @defgroup sysdef_parser (SysdefParser)
 */

/**
* @brief Initialize System definition from file.
* @ingroup dds_sysdef
* @component dds_sysdef_api
*
* Create dds_sysdef_system with provided system definition.
*
* @param[in] fp - Pointer to system definition file.
* @param[in,out] sysdef - Pointer dds_sysdef_system structure.
* @param[in] lib_scope - Library initialization mask.
*
* @return a DDS return code
*/
dds_return_t dds_sysdef_init_sysdef (FILE *fp, struct dds_sysdef_system **sysdef, uint32_t lib_scope);

/**
* @brief Initialize System definition from `xml` string.
* @ingroup dds_sysdef
* @component dds_sysdef_api
*
* Create dds_sysdef_system with provided system definition.
*
* @param[in] raw - System definition string.
* @param[in,out] sysdef - Pointer dds_sysdef_system structure.
* @param[in] lib_scope - Library initialization mask.
*
* @return a DDS return code
*/
dds_return_t dds_sysdef_init_sysdef_str (const char *raw, struct dds_sysdef_system **sysdef, uint32_t lib_scope);

/**
* @brief Finalize System definition.
* @ingroup dds_sysdef
* @component dds_sysdef_api
*
* Release resources allocated by dds_sysdef_system.
*
* @param[in] sysdef - Pointer to dds_sysdef_system structure.
*
*/
void dds_sysdef_fini_sysdef (struct dds_sysdef_system *sysdef);

/**
* @brief Initialize System definition for data types.
* @ingroup dds_sysdef
* @component dds_sysdef_api
*
* Create dds_sysdef_type_metadata_admin with provided system definition.
*
* @param[in] fp - Pointer to system definition file.
* @param[in,out] type_meta_data - Pointer dds_sysdef_type_metadata_admin structure.
*
* @return a DDS return code
*/
dds_return_t dds_sysdef_init_data_types (FILE *fp, struct dds_sysdef_type_metadata_admin **type_meta_data);

/**
* @brief Finalize System definition for data types.
* @ingroup dds_sysdef
* @component dds_sysdef_api
*
* Release resources allocated by dds_sysdef_system.
*
* @param[in,out] type_meta_data - Pointer dds_sysdef_type_metadata_admin structure.
*
*/
void dds_sysdef_fini_data_types (struct dds_sysdef_type_metadata_admin *type_meta_data);

#if defined (__cplusplus)
}
#endif

#endif // DDS_SYSDEF_PARSER_H
