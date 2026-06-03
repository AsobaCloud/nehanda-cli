#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "version_response.h"



static version_response_t *version_response_create_internal(
    char *version,
    char *service
    ) {
    version_response_t *version_response_local_var = malloc(sizeof(version_response_t));
    if (!version_response_local_var) {
        return NULL;
    }
    memset(version_response_local_var, 0, sizeof(version_response_t));
    version_response_local_var->_library_owned = 1;
    version_response_local_var->version = version;
    version_response_local_var->service = service;
    return version_response_local_var;
}

__attribute__((deprecated)) version_response_t *version_response_create(
    char *version,
    char *service
    ) {
    version_response_t *result = version_response_create_internal (
        version,
        service
        );
    if (!result) {
    }
    return result;
}

void version_response_free(version_response_t *version_response) {
    if(NULL == version_response){
        return ;
    }
    if(version_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "version_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (version_response->version) {
        free(version_response->version);
        version_response->version = NULL;
    }
    if (version_response->service) {
        free(version_response->service);
        version_response->service = NULL;
    }
    free(version_response);
}

cJSON *version_response_convertToJSON(version_response_t *version_response) {
    cJSON *item = cJSON_CreateObject();

    // version_response->version
    if (!version_response->version) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "version", version_response->version) == NULL) {
    goto fail; //String
    }


    // version_response->service
    if (!version_response->service) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "service", version_response->service) == NULL) {
    goto fail; //String
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

version_response_t *version_response_parseFromJSON(cJSON *version_responseJSON){

    version_response_t *version_response_local_var = NULL;

    char *version_local_str = NULL;

    char *service_local_str = NULL;

    // version_response->version
    cJSON *version = cJSON_GetObjectItemCaseSensitive(version_responseJSON, "version");
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

    // version_response->service
    cJSON *service = cJSON_GetObjectItemCaseSensitive(version_responseJSON, "service");
    if (cJSON_IsNull(service)) {
        service = NULL;
    }
    if (!service) {
        goto end;
    }

    
    if(!cJSON_IsString(service))
    {
    goto end; //String
    }


    if (version && !cJSON_IsNull(version)) version_local_str = strdup(version->valuestring);
    if (service && !cJSON_IsNull(service)) service_local_str = strdup(service->valuestring);

    version_response_local_var = version_response_create_internal (
        version_local_str,
        service_local_str
        );

    if (!version_response_local_var) {
        goto end;
    }

    return version_response_local_var;
end:
    if (version_local_str) {
        free(version_local_str);
        version_local_str = NULL;
    }
    if (service_local_str) {
        free(service_local_str);
        service_local_str = NULL;
    }
    return NULL;

}
