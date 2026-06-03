/*
 * search_hit.h
 *
 * 
 */

#ifndef _search_hit_H_
#define _search_hit_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct search_hit_t search_hit_t;

#include "search_hit_citations_inner.h"



typedef struct search_hit_t {
    char *artifact_id; // string
    double *score; //numeric
    char *kind; // string
    char *excerpt; // string
    list_t *citations; //nonprimitive container

    int _library_owned; // Is the library responsible for freeing this object?
} search_hit_t;

__attribute__((deprecated)) search_hit_t *search_hit_create(
    char *artifact_id,
    double *score,
    char *kind,
    char *excerpt,
    list_t *citations
);

void search_hit_free(search_hit_t *search_hit);

search_hit_t *search_hit_parseFromJSON(cJSON *search_hitJSON);

cJSON *search_hit_convertToJSON(search_hit_t *search_hit);

#endif /* _search_hit_H_ */

