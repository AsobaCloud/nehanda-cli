#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "create_release_request.h"



static create_release_request_t *create_release_request_create_internal(
    char *name
    ) {
    create_release_request_t *create_release_request_local_var = malloc(sizeof(create_release_request_t));
    if (!create_release_request_local_var) {
        return NULL;
    }
    memset(create_release_request_local_var, 0, sizeof(create_release_request_t));
    create_release_request_local_var->_library_owned = 1;
    create_release_request_local_var->name = name;
    return create_release_request_local_var;
}

__attribute__((deprecated)) create_release_request_t *create_release_request_create(
    char *name
    ) {
    create_release_request_t *result = create_release_request_create_internal (
        name
        );
    if (!result) {
    }
    return result;
}

void create_release_request_free(create_release_request_t *create_release_request) {
    if(NULL == create_release_request){
        return ;
    }
    if(create_release_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "create_release_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (create_release_request->name) {
        free(create_release_request->name);
        create_release_request->name = NULL;
    }
    free(create_release_request);
}

cJSON *create_release_request_convertToJSON(create_release_request_t *create_release_request) {
    cJSON *item = cJSON_CreateObject();

    // create_release_request->name
    if (!create_release_request->name) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "name", create_release_request->name) == NULL) {
    goto fail; //String
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

create_release_request_t *create_release_request_parseFromJSON(cJSON *create_release_requestJSON){

    create_release_request_t *create_release_request_local_var = NULL;

    char *name_local_str = NULL;

    // create_release_request->name
    cJSON *name = cJSON_GetObjectItemCaseSensitive(create_release_requestJSON, "name");
    if (cJSON_IsNull(name)) {
        name = NULL;
    }
    if (!name) {
        goto end;
    }

    
    if(!cJSON_IsString(name))
    {
    goto end; //String
    }


    if (name && !cJSON_IsNull(name)) name_local_str = strdup(name->valuestring);

    create_release_request_local_var = create_release_request_create_internal (
        name_local_str
        );

    if (!create_release_request_local_var) {
        goto end;
    }

    return create_release_request_local_var;
end:
    if (name_local_str) {
        free(name_local_str);
        name_local_str = NULL;
    }
    return NULL;

}
