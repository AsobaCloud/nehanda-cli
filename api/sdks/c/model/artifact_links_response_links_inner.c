#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "artifact_links_response_links_inner.h"



static artifact_links_response_links_inner_t *artifact_links_response_links_inner_create_internal(
    char *to_id,
    char *link_kind
    ) {
    artifact_links_response_links_inner_t *artifact_links_response_links_inner_local_var = malloc(sizeof(artifact_links_response_links_inner_t));
    if (!artifact_links_response_links_inner_local_var) {
        return NULL;
    }
    memset(artifact_links_response_links_inner_local_var, 0, sizeof(artifact_links_response_links_inner_t));
    artifact_links_response_links_inner_local_var->_library_owned = 1;
    artifact_links_response_links_inner_local_var->to_id = to_id;
    artifact_links_response_links_inner_local_var->link_kind = link_kind;
    return artifact_links_response_links_inner_local_var;
}

__attribute__((deprecated)) artifact_links_response_links_inner_t *artifact_links_response_links_inner_create(
    char *to_id,
    char *link_kind
    ) {
    artifact_links_response_links_inner_t *result = artifact_links_response_links_inner_create_internal (
        to_id,
        link_kind
        );
    if (!result) {
    }
    return result;
}

void artifact_links_response_links_inner_free(artifact_links_response_links_inner_t *artifact_links_response_links_inner) {
    if(NULL == artifact_links_response_links_inner){
        return ;
    }
    if(artifact_links_response_links_inner->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "artifact_links_response_links_inner_free");
        return ;
    }
    listEntry_t *listEntry;
    if (artifact_links_response_links_inner->to_id) {
        free(artifact_links_response_links_inner->to_id);
        artifact_links_response_links_inner->to_id = NULL;
    }
    if (artifact_links_response_links_inner->link_kind) {
        free(artifact_links_response_links_inner->link_kind);
        artifact_links_response_links_inner->link_kind = NULL;
    }
    free(artifact_links_response_links_inner);
}

cJSON *artifact_links_response_links_inner_convertToJSON(artifact_links_response_links_inner_t *artifact_links_response_links_inner) {
    cJSON *item = cJSON_CreateObject();

    // artifact_links_response_links_inner->to_id
    if(artifact_links_response_links_inner->to_id) {
    if(cJSON_AddStringToObject(item, "to_id", artifact_links_response_links_inner->to_id) == NULL) {
    goto fail; //String
    }
    }


    // artifact_links_response_links_inner->link_kind
    if(artifact_links_response_links_inner->link_kind) {
    if(cJSON_AddStringToObject(item, "link_kind", artifact_links_response_links_inner->link_kind) == NULL) {
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

artifact_links_response_links_inner_t *artifact_links_response_links_inner_parseFromJSON(cJSON *artifact_links_response_links_innerJSON){

    artifact_links_response_links_inner_t *artifact_links_response_links_inner_local_var = NULL;

    char *to_id_local_str = NULL;

    char *link_kind_local_str = NULL;

    // artifact_links_response_links_inner->to_id
    cJSON *to_id = cJSON_GetObjectItemCaseSensitive(artifact_links_response_links_innerJSON, "to_id");
    if (cJSON_IsNull(to_id)) {
        to_id = NULL;
    }
    if (to_id) { 
    if(!cJSON_IsString(to_id) && !cJSON_IsNull(to_id))
    {
    goto end; //String
    }
    }

    // artifact_links_response_links_inner->link_kind
    cJSON *link_kind = cJSON_GetObjectItemCaseSensitive(artifact_links_response_links_innerJSON, "link_kind");
    if (cJSON_IsNull(link_kind)) {
        link_kind = NULL;
    }
    if (link_kind) { 
    if(!cJSON_IsString(link_kind) && !cJSON_IsNull(link_kind))
    {
    goto end; //String
    }
    }


    if (to_id && !cJSON_IsNull(to_id)) to_id_local_str = strdup(to_id->valuestring);
    if (link_kind && !cJSON_IsNull(link_kind)) link_kind_local_str = strdup(link_kind->valuestring);

    artifact_links_response_links_inner_local_var = artifact_links_response_links_inner_create_internal (
        to_id_local_str,
        link_kind_local_str
        );

    if (!artifact_links_response_links_inner_local_var) {
        goto end;
    }

    return artifact_links_response_links_inner_local_var;
end:
    if (to_id_local_str) {
        free(to_id_local_str);
        to_id_local_str = NULL;
    }
    if (link_kind_local_str) {
        free(link_kind_local_str);
        link_kind_local_str = NULL;
    }
    return NULL;

}
