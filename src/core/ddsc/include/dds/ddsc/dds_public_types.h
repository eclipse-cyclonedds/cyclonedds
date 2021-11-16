// MAKI TODO: licence header

#ifndef _DDS_PUBLIC_TYPES_H_
#define _DDS_PUBLIC_TYPES_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Handle to an entity. A valid entity handle will always have a positive
 * integer value. Should the value be negative, it is one of the DDS_RETCODE_*
 * error codes.
 */
typedef int32_t dds_entity_t;

// MAKI we need entity_t to be available separately, the headers included
// afterwards partly rely on it ...
// TODO : add other types from dds.h if it is reasonable

#endif /*_DDS_PUBLIC_TYPES_H_*/