/**
 * nehanda_config.c — Configuration resolution for nehanda-cli.
 *
 * Resolves in priority order:
 *   1. Environment variable (NEHANDA_API_KEY, NEHANDA_GATEWAY_URL)
 *   2. nehanda.yaml in the project root or ~/.config/nehanda/nehanda.yaml
 *   3. Session token from DB1 (set by nehanda_auth.c after login)
 *   4. Built-in defaults (gateway: https://gateway.nehanda.co)
 *
 * Self-host mode: if NEHANDA_SELF_HOST_ENDPOINT is set (or self_host.endpoint
 * in nehanda.yaml), the gateway URL and API key are ignored entirely and all
 * upstream calls go directly to that endpoint.
 *
 * Exposes:
 *   nehanda_config_t  nehanda_config_load(void)
 *   const char*       nehanda_get_gateway_url(void)
 *   const char*       nehanda_get_api_key(void)    — never logs the value
 *   bool              nehanda_is_self_hosted(void)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define NEHANDA_DEFAULT_GATEWAY_URL "https://gateway.nehanda.co"
#define NEHANDA_DEFAULT_MODEL       "nehanda-rag-synthesis-27b"

typedef struct {
    char gateway_url[512];
    char model[256];
    char api_key[1024];   /* resolved at runtime; never persisted in plaintext */
    bool self_hosted;
    char self_host_endpoint[512];
} nehanda_config_t;

/* TODO: implement nehanda_config_load()     */
/* TODO: implement nehanda_get_gateway_url() */
/* TODO: implement nehanda_get_api_key()     */
/* TODO: implement nehanda_is_self_hosted()  */
