#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windows.h>
#include "bcrypt.h"
#include <string.h>
#include "dds/ddsrt/random.h"

bool ddsrt_prng_makeseed (struct ddsrt_prng_seed *seed)
{
  NTSTATUS res;
  memset (seed->key, 0, sizeof (seed->key));
  res = BCryptGenRandom (NULL, (PUCHAR) seed->key, (ULONG) sizeof (seed->key), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  return (res >= 0);
}
