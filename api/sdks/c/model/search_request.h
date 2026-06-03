/*
 * search_request.h
 *
 * 
 */

#ifndef _search_request_H_
#define _search_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct search_request_t search_request_t;




typedef struct search_request_t {
    char *query; // string
    char *project; // string
    char *scope_kind; // string
    char *scope_id; // string
    int *max_results; //numeric
    char *fusion_mode; // string
    char *cursor; // string

    int _library_owned; // Is the library responsible for freeing this object?
} search_request_t;

__attribute__((deprecated)) search_request_t *search_request_create(
    char *query,
    char *project,
    char *scope_kind,
    char *scope_id,
    int *max_results,
    char *fusion_mode,
    char *cursor
);

void search_request_free(search_request_t *search_request);

search_request_t *search_request_parseFromJSON(cJSON *search_requestJSON);

cJSON *search_request_convertToJSON(search_request_t *search_request);

#endif /* _search_request_H_ */

