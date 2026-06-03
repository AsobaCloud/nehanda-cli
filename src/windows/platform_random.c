/* platform_random.c: BCryptGenRandom random bytes (Windows) */
#include "platform_random.h"
#include <bcrypt.h>

int platform_random_bytes(void *buf, size_t len)
{
   /* NT_SUCCESS(x) == ((x) >= 0), avoid depending on ntstatus.h */
   NTSTATUS status =
       BCryptGenRandom(NULL, (PUCHAR)buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
   return (status >= 0) ? 0 : -1;
}
