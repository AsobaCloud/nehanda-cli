/*
 * blast_radius_response.h
 *
 * 
 */

#ifndef _blast_radius_response_H_
#define _blast_radius_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct blast_radius_response_t blast_radius_response_t;




typedef struct blast_radius_response_t {
    char *file; // string
    list_t *dependents; //primitive container
    int *dependent_count; //numeric
    list_t *dependencies; //primitive container
    int *dependency_count; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} blast_radius_response_t;

__attribute__((deprecated)) blast_radius_response_t *blast_radius_response_create(
    char *file,
    list_t *dependents,
    int *dependent_count,
    list_t *dependencies,
    int *dependency_count
);

void blast_radius_response_free(blast_radius_response_t *blast_radius_response);

blast_radius_response_t *blast_radius_response_parseFromJSON(cJSON *blast_radius_responseJSON);

cJSON *blast_radius_response_convertToJSON(blast_radius_response_t *blast_radius_response);

#endif /* _blast_radius_response_H_ */

