#ifndef CLI_V1_ROUTES_H
#define CLI_V1_ROUTES_H
/* INTERNAL: /v1 thin-client routing entry points. Shared by posix+windows
 * cli_client.c and the delegate test. Impl in cli_v1_routes*.c (split TUs). */
#include "cli_client.h"
#define V1_PROTOCOL_VERSION 1
const char *json_str(cJSON *obj, const char *key);
const char *cli_v1_route_for_method(const char *method, const char **verb_out);
const char *cli_v1_pathid_route_for_method(const char *method, const char **verb_out, const char **suffix_out, const char **id_field_out);
cJSON *cli_v1_dispatch_local(cJSON *req, int timeout_ms);
#endif
