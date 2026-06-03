#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "capabilities_response.h"



static capabilities_response_t *capabilities_response_create_internal(
    list_t *capabilities,
    char *version
    ) {
    capabilities_response_t *capabilities_response_local_var = malloc(sizeof(capabilities_response_t));
    if (!capabilities_response_local_var) {
        return NULL;
    }
    memset(capabilities_response_local_var, 0, sizeof(capabilities_response_t));
    capabilities_response_local_var->_library_owned = 1;
    capabilities_response_local_var->capabilities = capabilities;
    capabilities_response_local_var->version = version;
    return capabilities_response_local_var;
}

__attribute__((deprecated)) capabilities_response_t *capabilities_response_create(
    list_t *capabilities,
    char *version
    ) {
    capabilities_response_t *result = capabilities_response_create_internal (
        capabilities,
        version
        );
    if (!result) {
    }
    return result;
}

void capabilities_response_free(capabilities_response_t *capabilities_response) {
    if(NULL == capabilities_response){
        return ;
    }
    if(capabilities_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "capabilities_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (capabilities_response->capabilities) {
        list_ForEach(listEntry, capabilities_response->capabilities) {
            free(listEntry->data);
        }
        list_freeList(capabilities_response->capabilities);
        capabilities_response->capabilities = NULL;
    }
    if (capabilities_response->version) {
        free(capabilities_response->version);
        capabilities_response->version = NULL;
    }
    free(capabilities_response);
}

cJSON *capabilities_response_convertToJSON(capabilities_response_t *capabilities_response) {
    cJSON *item = cJSON_CreateObject();

    // capabilities_response->capabilities
    if (!capabilities_response->capabilities) {
        goto fail;
    }
    cJSON *capabilities = cJSON_AddArrayToObject(item, "capabilities");
    if(capabilities == NULL) {
        goto fail; //primitive container
    }

    listEntry_t *capabilitiesListEntry;
    list_ForEach(capabilitiesListEntry, capabilities_response->capabilities) {
    if(cJSON_AddStringToObject(capabilities, "", capabilitiesListEntry->data) == NULL)
    {
        goto fail;
    }
    }


    // capabilities_response->version
    if (!capabilities_response->version) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "version", capabilities_response->version) == NULL) {
    goto fail; //String
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

capabilities_response_t *capabilities_response_parseFromJSON(cJSON *capabilities_responseJSON){

    capabilities_response_t *capabilities_response_local_var = NULL;

    // define the local list for capabilities_response->capabilities
    list_t *capabilitiesList = NULL;

    char *version_local_str = NULL;

    // capabilities_response->capabilities
    cJSON *capabilities = cJSON_GetObjectItemCaseSensitive(capabilities_responseJSON, "capabilities");
    if (cJSON_IsNull(capabilities)) {
        capabilities = NULL;
    }
    if (!capabilities) {
        goto end;
    }

    
    cJSON *capabilities_local = NULL;
    if(!cJSON_IsArray(capabilities)) {
        goto end;//primitive container
    }
    capabilitiesList = list_createList();

    cJSON_ArrayForEach(capabilities_local, capabilities)
    {
        if(!cJSON_IsString(capabilities_local))
        {
            goto end;
        }
        list_addElement(capabilitiesList , strdup(capabilities_local->valuestring));
    }

    // capabilities_response->version
    cJSON *version = cJSON_GetObjectItemCaseSensitive(capabilities_responseJSON, "version");
    if (cJSON_IsNull(version)) {
        version = NULL;
    }
    if (!version) {
        goto end;
    }

    
    if(!cJSON_IsString(version))
    {
    goto end; //String
    }


    if (version && !cJSON_IsNull(version)) version_local_str = strdup(version->valuestring);

    capabilities_response_local_var = capabilities_response_create_internal (
        capabilitiesList,
        version_local_str
        );

    if (!capabilities_response_local_var) {
        goto end;
    }

    return capabilities_response_local_var;
end:
    if (capabilitiesList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, capabilitiesList) {
            free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(capabilitiesList);
        capabilitiesList = NULL;
    }
    if (version_local_str) {
        free(version_local_str);
        version_local_str = NULL;
    }
    return NULL;

}
