#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "artifact_links_response.h"



static artifact_links_response_t *artifact_links_response_create_internal(
    char *artifact_id,
    list_t *links
    ) {
    artifact_links_response_t *artifact_links_response_local_var = malloc(sizeof(artifact_links_response_t));
    if (!artifact_links_response_local_var) {
        return NULL;
    }
    memset(artifact_links_response_local_var, 0, sizeof(artifact_links_response_t));
    artifact_links_response_local_var->_library_owned = 1;
    artifact_links_response_local_var->artifact_id = artifact_id;
    artifact_links_response_local_var->links = links;
    return artifact_links_response_local_var;
}

__attribute__((deprecated)) artifact_links_response_t *artifact_links_response_create(
    char *artifact_id,
    list_t *links
    ) {
    artifact_links_response_t *result = artifact_links_response_create_internal (
        artifact_id,
        links
        );
    if (!result) {
    }
    return result;
}

void artifact_links_response_free(artifact_links_response_t *artifact_links_response) {
    if(NULL == artifact_links_response){
        return ;
    }
    if(artifact_links_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "artifact_links_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (artifact_links_response->artifact_id) {
        free(artifact_links_response->artifact_id);
        artifact_links_response->artifact_id = NULL;
    }
    if (artifact_links_response->links) {
        list_ForEach(listEntry, artifact_links_response->links) {
            artifact_links_response_links_inner_free(listEntry->data);
        }
        list_freeList(artifact_links_response->links);
        artifact_links_response->links = NULL;
    }
    free(artifact_links_response);
}

cJSON *artifact_links_response_convertToJSON(artifact_links_response_t *artifact_links_response) {
    cJSON *item = cJSON_CreateObject();

    // artifact_links_response->artifact_id
    if(artifact_links_response->artifact_id) {
    if(cJSON_AddStringToObject(item, "artifact_id", artifact_links_response->artifact_id) == NULL) {
    goto fail; //String
    }
    }


    // artifact_links_response->links
    if(artifact_links_response->links) {
    cJSON *links = cJSON_AddArrayToObject(item, "links");
    if(links == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *linksListEntry;
    if (artifact_links_response->links) {
    list_ForEach(linksListEntry, artifact_links_response->links) {
    cJSON *itemLocal = artifact_links_response_links_inner_convertToJSON(linksListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(links, itemLocal);
    }
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

artifact_links_response_t *artifact_links_response_parseFromJSON(cJSON *artifact_links_responseJSON){

    artifact_links_response_t *artifact_links_response_local_var = NULL;

    char *artifact_id_local_str = NULL;

    // define the local list for artifact_links_response->links
    list_t *linksList = NULL;

    // artifact_links_response->artifact_id
    cJSON *artifact_id = cJSON_GetObjectItemCaseSensitive(artifact_links_responseJSON, "artifact_id");
    if (cJSON_IsNull(artifact_id)) {
        artifact_id = NULL;
    }
    if (artifact_id) { 
    if(!cJSON_IsString(artifact_id) && !cJSON_IsNull(artifact_id))
    {
    goto end; //String
    }
    }

    // artifact_links_response->links
    cJSON *links = cJSON_GetObjectItemCaseSensitive(artifact_links_responseJSON, "links");
    if (cJSON_IsNull(links)) {
        links = NULL;
    }
    if (links) { 
    cJSON *links_local_nonprimitive = NULL;
    if(!cJSON_IsArray(links)){
        goto end; //nonprimitive container
    }

    linksList = list_createList();

    cJSON_ArrayForEach(links_local_nonprimitive,links )
    {
        if(!cJSON_IsObject(links_local_nonprimitive)){
            goto end;
        }
        artifact_links_response_links_inner_t *linksItem = artifact_links_response_links_inner_parseFromJSON(links_local_nonprimitive);

        list_addElement(linksList, linksItem);
    }
    }


    if (artifact_id && !cJSON_IsNull(artifact_id)) artifact_id_local_str = strdup(artifact_id->valuestring);

    artifact_links_response_local_var = artifact_links_response_create_internal (
        artifact_id_local_str,
        links ? linksList : NULL
        );

    if (!artifact_links_response_local_var) {
        goto end;
    }

    return artifact_links_response_local_var;
end:
    if (artifact_id_local_str) {
        free(artifact_id_local_str);
        artifact_id_local_str = NULL;
    }
    if (linksList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, linksList) {
            artifact_links_response_links_inner_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(linksList);
        linksList = NULL;
    }
    return NULL;

}
