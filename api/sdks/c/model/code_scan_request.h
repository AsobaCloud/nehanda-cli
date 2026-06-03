/*
 * code_scan_request.h
 *
 * 
 */

#ifndef _code_scan_request_H_
#define _code_scan_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct code_scan_request_t code_scan_request_t;




typedef struct code_scan_request_t {
    char *project; // string
    char *root_path; // string
    int *force; //boolean

    int _library_owned; // Is the library responsible for freeing this object?
} code_scan_request_t;

__attribute__((deprecated)) code_scan_request_t *code_scan_request_create(
    char *project,
    char *root_path,
    int *force
);

void code_scan_request_free(code_scan_request_t *code_scan_request);

code_scan_request_t *code_scan_request_parseFromJSON(cJSON *code_scan_requestJSON);

cJSON *code_scan_request_convertToJSON(code_scan_request_t *code_scan_request);

#endif /* _code_scan_request_H_ */

