// Copyright(c) 2026 IntervalZero, Inc
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

///////////////////////////////////////////////////////////////////////////////
// Includes
///////////////////////////////////////////////////////////////////////////////

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <RtApi.h>
#include <RtssApi.h>

#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/retcode.h"

///////////////////////////////////////////////////////////////////////////////
// Private data types
///////////////////////////////////////////////////////////////////////////////

typedef struct NODE_ENTRY_
{
    struct NODE_ENTRY_ *pNext;
} NODE_ENTRY;

typedef struct RECORDED_ENV_VARIABLE_
{
    NODE_ENTRY Node;
    CHAR *pName;
    CHAR *pValue;
} RECORDED_ENV_VARIABLE;

///////////////////////////////////////////////////////////////////////////////
// Private functions declaration
///////////////////////////////////////////////////////////////////////////////

static dds_return_t GetStringRegistryEntry(HKEY,const char*,const char*,
                                           const char**);

///////////////////////////////////////////////////////////////////////////////
// Private global variables
///////////////////////////////////////////////////////////////////////////////

static NODE_ENTRY *pRecordedEnvVariablesListHead;

///////////////////////////////////////////////////////////////////////////////
// Public functions
///////////////////////////////////////////////////////////////////////////////

dds_return_t
ddsrt_getenv(const char *name, const char **value)
{
  // Declare local variables
  dds_return_t ret;
  CHAR *pName;
  CHAR *pValue;

  // Verify prerequisites
  assert(name != NULL);
  assert(value != NULL);

  // Try to find the requested environment variable in our list of already
  // read environment variables.
  for(NODE_ENTRY *pNode=pRecordedEnvVariablesListHead;
      pNode;
      pNode=pNode->pNext)
  {
    RECORDED_ENV_VARIABLE *pRecordedEnvVariable =
                             CONTAINING_RECORD(pNode,
                                               RECORDED_ENV_VARIABLE,
                                               Node);
    if (!strcmp(pRecordedEnvVariable->pName, name)) {
      *value = pRecordedEnvVariable->pValue;
      return DDS_RETCODE_OK;
    }
  }

  // Try to read from the system defined environment variables.
  // (we cannot access the user defined environment variables from RTSS)
  ret = GetStringRegistryEntry(
          HKEY_LOCAL_MACHINE, // HiveKey
          "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment", // pSubKeyName
          name, // pEntryName,
          &pValue); // ppEntryValue
  if (ret != DDS_RETCODE_OK) {
    goto end;
  }

  // Here, the environment variable was found.
  // Allocate memory to store the name of the environment variable
  ULONG NameLength = (ULONG)strlen(name);
  pName = (CHAR*)RtAllocateLocalMemory(NameLength+1);
  if (!pName) {
    RtFreeLocalMemory(pValue);
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
    goto end;
  }
  memcpy(pName, name, NameLength);
  pName[NameLength] = '\0';

  // Allocate memory for the env variable data structure.
  RECORDED_ENV_VARIABLE *pRecordedEnvVariable =
               (RECORDED_ENV_VARIABLE*)RtAllocateLocalMemory(
                                                sizeof(RECORDED_ENV_VARIABLE));
  if (!pRecordedEnvVariable) {
    RtFreeLocalMemory(pName);
    RtFreeLocalMemory(pValue);
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
    goto end;
  }
  pRecordedEnvVariable->pName = pName;
  pRecordedEnvVariable->pValue = pValue;

  // Insert the env variable data structure in our list of recorded
  // environment variables.
  pRecordedEnvVariable->Node.pNext = pRecordedEnvVariablesListHead;
  pRecordedEnvVariablesListHead = &pRecordedEnvVariable->Node;

  // Here, the environment variable was not found.
  *value = pRecordedEnvVariable->pValue;
  ret = DDS_RETCODE_OK;

  end:
  return ret;
}

dds_return_t
ddsrt_setenv(const char *name, const char *value)
{
  assert(FALSE);
  return DDS_RETCODE_UNSUPPORTED;
}

dds_return_t
ddsrt_unsetenv(const char *name)
{
  assert(FALSE);
  return DDS_RETCODE_UNSUPPORTED;
}

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////

// This function allocates memory dynamically for the entry value,
// so the caller is responsible for freeing it.
static dds_return_t
GetStringRegistryEntry(HKEY HiveKey,
                       const char *pSubKeyName,
                       const char *pEntryName,
                       const char **ppEntryValue)
{
  // Declare local variables
  HKEY hSubKey;
  DWORD ValueType;
  DWORD DataSize;
  CHAR *pEntryValue;
  LSTATUS Status;
  dds_return_t ret;

  // Get a handle to the sub key
  Status = RegOpenKeyExA(
             HiveKey, // hKey
             pSubKeyName, // lpSubkey
             0, // ulOptions
             0, // samDesired
             &hSubKey); // phkResult
  if (Status != ERROR_SUCCESS) {
    ret = DDS_RETCODE_NOT_FOUND;
    goto end;
  }

  // Make a first call to RegQueryValueExA to get the entry size
  DataSize = 0;
  Status = RegQueryValueExA(
             hSubKey, // hKey
             pEntryName, // lpValueName
             NULL, // lpReserved
             &ValueType, // lpType
             NULL, // lpData
             &DataSize); // lpcbData
  if (Status != ERROR_SUCCESS || ValueType != REG_SZ) {
    RegCloseKey(hSubKey);
    ret = DDS_RETCODE_NOT_FOUND;
    goto end;
  }

  // Allocate and clear memory for the entry value
  pEntryValue = RtAllocateLocalMemory(DataSize+1);
  if (!pEntryValue) {
    RegCloseKey(hSubKey);
    ret = DDS_RETCODE_NOT_FOUND;
    goto end;
  }
  memset(pEntryValue, 0, DataSize+1);

  // Now, read the value into the allocated buffer
  Status = RegQueryValueExA(
             hSubKey, // hKey
             pEntryName, // lpValueName
             NULL, // lpReserved
             &ValueType, // lpType
             (LPBYTE)pEntryValue, // lpData
             &DataSize); // lpcbData
  if (Status != ERROR_SUCCESS || ValueType != REG_SZ) {
    RtFreeLocalMemory(pEntryValue);
    RegCloseKey(hSubKey);
    ret = DDS_RETCODE_NOT_FOUND;
    goto end;
  }

  // Close the handle to the subkey
  RegCloseKey(hSubKey);

  // Here, everything went well
  *ppEntryValue = pEntryValue;
  ret = DDS_RETCODE_OK;

  end:
  return ret;
}

