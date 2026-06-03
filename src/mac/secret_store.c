/* secret_store.c: macOS Keychain backend */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include "secrets_internal.h"
#include "log.h"
#include <Security/Security.h>
#include <stdio.h>
#include <string.h>

static int keychain_store(const char *service, const char *key, const char *value)
{
   OSStatus status;
   size_t val_len = strlen(value);

   /* Try to update first */
   SecKeychainItemRef item = NULL;
   status = SecKeychainFindGenericPassword(NULL, (UInt32)strlen(service), service,
                                           (UInt32)strlen(key), key, NULL, NULL, &item);
   if (status == errSecSuccess && item)
   {
      status = SecKeychainItemModifyAttributesAndData(item, NULL, (UInt32)val_len, value);
      CFRelease(item);
      return (status == errSecSuccess) ? 0 : -1;
   }

   /* Add new */
   status = SecKeychainAddGenericPassword(NULL, (UInt32)strlen(service), service,
                                          (UInt32)strlen(key), key, (UInt32)val_len, value, NULL);
   return (status == errSecSuccess) ? 0 : -1;
}

static int keychain_load(const char *service, const char *key, char *buf, size_t len)
{
   UInt32 pw_len = 0;
   void *pw_data = NULL;
   OSStatus status = SecKeychainFindGenericPassword(
       NULL, (UInt32)strlen(service), service, (UInt32)strlen(key), key, &pw_len, &pw_data, NULL);
   if (status != errSecSuccess || !pw_data)
      return -1;

   size_t copy = pw_len < len - 1 ? pw_len : len - 1;
   memcpy(buf, pw_data, copy);
   buf[copy] = '\0';
   SecKeychainItemFreeContent(NULL, pw_data);
   return 0;
}

static int keychain_remove(const char *service, const char *key)
{
   SecKeychainItemRef item = NULL;
   OSStatus status = SecKeychainFindGenericPassword(NULL, (UInt32)strlen(service), service,
                                                    (UInt32)strlen(key), key, NULL, NULL, &item);
   if (status != errSecSuccess || !item)
      return -1;
   status = SecKeychainItemDelete(item);
   CFRelease(item);
   return (status == errSecSuccess) ? 0 : -1;
}

static const db1_secret_backend_t keychain_backend = {
    .store = keychain_store,
    .load = keychain_load,
    .remove = keychain_remove,
    .name = "keychain",
};

const db1_secret_backend_t *db1_secret_backend_detect(const db1_secret_backend_t *file_fallback)
{
   /* Test keychain access with a dummy lookup (returns errSecItemNotFound if accessible) */
   SecKeychainItemRef item = NULL;
   OSStatus status =
       SecKeychainFindGenericPassword(NULL, 5, "aimee", 5, "_test", NULL, NULL, &item);
   if (status == errSecSuccess || status == errSecItemNotFound)
   {
      LOG_DEBUG("secret", "secret backend: keychain");
      if (item)
         CFRelease(item);
      return &keychain_backend;
   }
   LOG_DEBUG("secret", "secret backend: file");
   return file_fallback;
}

#pragma clang diagnostic pop
