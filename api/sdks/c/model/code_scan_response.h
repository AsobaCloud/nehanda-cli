/*
 * code_scan_response.h
 *
 * 
 */

#ifndef _code_scan_response_H_
#define _code_scan_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct code_scan_response_t code_scan_response_t;




typedef struct code_scan_response_t {
    char *status; // string
    int *skipped; //boolean
    char *project; // string
    int *files; //numeric
    int *inspected; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} code_scan_response_t;

__attribute__((deprecated)) code_scan_response_t *code_scan_response_create(
    char *status,
    int *skipped,
    char *project,
    int *files,
    int *inspected
);

void code_scan_response_free(code_scan_response_t *code_scan_response);

code_scan_response_t *code_scan_response_parseFromJSON(cJSON *code_scan_responseJSON);

cJSON *code_scan_response_convertToJSON(code_scan_response_t *code_scan_response);

#endif /* _code_scan_response_H_ */

