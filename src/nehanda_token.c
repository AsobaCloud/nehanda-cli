#include "nehanda_token.h"

#include <openssl/sha.h>
#include <stdio.h>
#include <string.h>

#define PERSISTENT_SALT "nehanda-sovereign-salt-2026"

int validate_client_access_token(const char *provided_token)
{
    if (!provided_token || strlen(provided_token) == 0)
        return -1;

    const char *secret_key = getenv("NEHANDA_SECRET_KEY");

    char fallback_buffer[256];
    if (!secret_key) {
        const char *user = getenv("USER");
        if (!user)
            user = "default-nehanda-userspace";
        snprintf(fallback_buffer, sizeof(fallback_buffer), "%s:%s", user, PERSISTENT_SALT);
        secret_key = fallback_buffer;
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, secret_key, strlen(secret_key));
    SHA256_Final(hash, &sha256);

    char expected_token_hex[SHA256_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(&expected_token_hex[i * 2], "%02x", hash[i]);

    if (strcmp(provided_token, expected_token_hex) == 0)
        return 0;

    return -1;
}
