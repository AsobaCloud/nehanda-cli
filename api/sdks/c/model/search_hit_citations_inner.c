#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "search_hit_citations_inner.h"



static search_hit_citations_inner_t *search_hit_citations_inner_create_internal(
    char *source_kind,
    char *source_id
    ) {
    search_hit_citations_inner_t *search_hit_citations_inner_local_var = malloc(sizeof(search_hit_citations_inner_t));
    if (!search_hit_citations_inner_local_var) {
        return NULL;
    }
    memset(search_hit_citations_inner_local_var, 0, sizeof(search_hit_citations_inner_t));
    search_hit_citations_inner_local_var->_library_owned = 1;
    search_hit_citations_inner_local_var->source_kind = source_kind;
    search_hit_citations_inner_local_var->source_id = source_id;
    return search_hit_citations_inner_local_var;
}

__attribute__((deprecated)) search_hit_citations_inner_t *search_hit_citations_inner_create(
    char *source_kind,
    char *source_id
    ) {
    search_hit_citations_inner_t *result = search_hit_citations_inner_create_internal (
        source_kind,
        source_id
        );
    if (!result) {
    }
    return result;
}

void search_hit_citations_inner_free(search_hit_citations_inner_t *search_hit_citations_inner) {
    if(NULL == search_hit_citations_inner){
        return ;
    }
    if(search_hit_citations_inner->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "search_hit_citations_inner_free");
        return ;
    }
    listEntry_t *listEntry;
    if (search_hit_citations_inner->source_kind) {
        free(search_hit_citations_inner->source_kind);
        search_hit_citations_inner->source_kind = NULL;
    }
    if (search_hit_citations_inner->source_id) {
        free(search_hit_citations_inner->source_id);
        search_hit_citations_inner->source_id = NULL;
    }
    free(search_hit_citations_inner);
}

cJSON *search_hit_citations_inner_convertToJSON(search_hit_citations_inner_t *search_hit_citations_inner) {
    cJSON *item = cJSON_CreateObject();

    // search_hit_citations_inner->source_kind
    if(search_hit_citations_inner->source_kind) {
    if(cJSON_AddStringToObject(item, "source_kind", search_hit_citations_inner->source_kind) == NULL) {
    goto fail; //String
    }
    }


    // search_hit_citations_inner->source_id
    if(search_hit_citations_inner->source_id) {
    if(cJSON_AddStringToObject(item, "source_id", search_hit_citations_inner->source_id) == NULL) {
    goto fail; //String
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

search_hit_citations_inner_t *search_hit_citations_inner_parseFromJSON(cJSON *search_hit_citations_innerJSON){

    search_hit_citations_inner_t *search_hit_citations_inner_local_var = NULL;

    char *source_kind_local_str = NULL;

    char *source_id_local_str = NULL;

    // search_hit_citations_inner->source_kind
    cJSON *source_kind = cJSON_GetObjectItemCaseSensitive(search_hit_citations_innerJSON, "source_kind");
    if (cJSON_IsNull(source_kind)) {
        source_kind = NULL;
    }
    if (source_kind) { 
    if(!cJSON_IsString(source_kind) && !cJSON_IsNull(source_kind))
    {
    goto end; //String
    }
    }

    // search_hit_citations_inner->source_id
    cJSON *source_id = cJSON_GetObjectItemCaseSensitive(search_hit_citations_innerJSON, "source_id");
    if (cJSON_IsNull(source_id)) {
        source_id = NULL;
    }
    if (source_id) { 
    if(!cJSON_IsString(source_id) && !cJSON_IsNull(source_id))
    {
    goto end; //String
    }
    }


    if (source_kind && !cJSON_IsNull(source_kind)) source_kind_local_str = strdup(source_kind->valuestring);
    if (source_id && !cJSON_IsNull(source_id)) source_id_local_str = strdup(source_id->valuestring);

    search_hit_citations_inner_local_var = search_hit_citations_inner_create_internal (
        source_kind_local_str,
        source_id_local_str
        );

    if (!search_hit_citations_inner_local_var) {
        goto end;
    }

    return search_hit_citations_inner_local_var;
end:
    if (source_kind_local_str) {
        free(source_kind_local_str);
        source_kind_local_str = NULL;
    }
    if (source_id_local_str) {
        free(source_id_local_str);
        source_id_local_str = NULL;
    }
    return NULL;

}
