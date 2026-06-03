/*
 * search_hit_citations_inner.h
 *
 * 
 */

#ifndef _search_hit_citations_inner_H_
#define _search_hit_citations_inner_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct search_hit_citations_inner_t search_hit_citations_inner_t;




typedef struct search_hit_citations_inner_t {
    char *source_kind; // string
    char *source_id; // string

    int _library_owned; // Is the library responsible for freeing this object?
} search_hit_citations_inner_t;

__attribute__((deprecated)) search_hit_citations_inner_t *search_hit_citations_inner_create(
    char *source_kind,
    char *source_id
);

void search_hit_citations_inner_free(search_hit_citations_inner_t *search_hit_citations_inner);

search_hit_citations_inner_t *search_hit_citations_inner_parseFromJSON(cJSON *search_hit_citations_innerJSON);

cJSON *search_hit_citations_inner_convertToJSON(search_hit_citations_inner_t *search_hit_citations_inner);

#endif /* _search_hit_citations_inner_H_ */

