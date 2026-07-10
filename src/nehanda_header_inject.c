/**
 * nehanda_header_inject.c — Auth header injection for upstream Nehanda calls.
 *
 * Wraps aimee's upstream/src/provider_client.c HTTP layer.
 * Before every request to the Nehanda Gateway, this module:
 *   1. Reads the current API key via nehanda_config.c (env > yaml > DB1 session)
 *   2. If the token is expired, triggers nehanda_auth.c re-auth flow
 *   3. Injects the resolved key as:
 *        Authorization: Bearer <token>
 *      (aimee's native "extra HTTP header injection" in agent_http.c)
 *
 * The Gateway reads this header, validates it against ona-user-auth Lambda,
 * and checks the nehanda subscription tier in ona-platform-users DynamoDB.
 *
 * On 401 response: triggers re-auth automatically, retries once.
 * On 402/429 response: surfaces quota/billing error to the user with
 *   a link to https://auth.ona-platform.co/upgrade.
 *
 * This module only applies to calls whose destination matches
 * nehanda_get_gateway_url(). Local delegate calls (Ollama) are never
 * intercepted.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* TODO: implement nehanda_should_inject(const char* url) */
/* TODO: implement nehanda_inject_auth_header(http_request_t* req) */
/* TODO: implement nehanda_handle_auth_error(int http_status)      */
