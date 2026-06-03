#include "kb_doc_hash.h"

#include <openssl/sha.h>
#include <stdio.h>

void kb_doc_content_hash(const char *bytes, int nbytes, char out[KB_DOC_HASH_HEX_LEN + 1])
{
   unsigned char digest[SHA256_DIGEST_LENGTH];

   if (!out)
      return;

   if (!bytes || nbytes < 0)
      nbytes = 0;

   SHA256((const unsigned char *)(bytes ? bytes : ""), (size_t)nbytes, digest);
   for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
      snprintf(out + i * 2, 3, "%02x", digest[i]);
   out[KB_DOC_HASH_HEX_LEN] = '\0';
}
