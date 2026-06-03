/*
 * workers_response.h
 *
 * 
 */

#ifndef _workers_response_H_
#define _workers_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct workers_response_t workers_response_t;

#include "object.h"



typedef struct workers_response_t {
    char *status; // string
    int *configured; //numeric
    list_t *slots; //nonprimitive container
    list_t *threads; //nonprimitive container
    list_t *background; //nonprimitive container

    int _library_owned; // Is the library responsible for freeing this object?
} workers_response_t;

__attribute__((deprecated)) workers_response_t *workers_response_create(
    char *status,
    int *configured,
    list_t *slots,
    list_t *threads,
    list_t *background
);

void workers_response_free(workers_response_t *workers_response);

workers_response_t *workers_response_parseFromJSON(cJSON *workers_responseJSON);

cJSON *workers_response_convertToJSON(workers_response_t *workers_response);

#endif /* _workers_response_H_ */

