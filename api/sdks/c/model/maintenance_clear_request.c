#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "maintenance_clear_request.h"



static maintenance_clear_request_t *maintenance_clear_request_create_internal(
    char *project
    ) {
    maintenance_clear_request_t *maintenance_clear_request_local_var = malloc(sizeof(maintenance_clear_request_t));
    if (!maintenance_clear_request_local_var) {
        return NULL;
    }
    memset(maintenance_clear_request_local_var, 0, sizeof(maintenance_clear_request_t));
    maintenance_clear_request_local_var->_library_owned = 1;
    maintenance_clear_request_local_var->project = project;
    return maintenance_clear_request_local_var;
}

__attribute__((deprecated)) maintenance_clear_request_t *maintenance_clear_request_create(
    char *project
    ) {
    maintenance_clear_request_t *result = maintenance_clear_request_create_internal (
        project
        );
    if (!result) {
    }
    return result;
}

void maintenance_clear_request_free(maintenance_clear_request_t *maintenance_clear_request) {
    if(NULL == maintenance_clear_request){
        return ;
    }
    if(maintenance_clear_request->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "maintenance_clear_request_free");
        return ;
    }
    listEntry_t *listEntry;
    if (maintenance_clear_request->project) {
        free(maintenance_clear_request->project);
        maintenance_clear_request->project = NULL;
    }
    free(maintenance_clear_request);
}

cJSON *maintenance_clear_request_convertToJSON(maintenance_clear_request_t *maintenance_clear_request) {
    cJSON *item = cJSON_CreateObject();

    // maintenance_clear_request->project
    if (!maintenance_clear_request->project) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "project", maintenance_clear_request->project) == NULL) {
    goto fail; //String
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

maintenance_clear_request_t *maintenance_clear_request_parseFromJSON(cJSON *maintenance_clear_requestJSON){

    maintenance_clear_request_t *maintenance_clear_request_local_var = NULL;

    char *project_local_str = NULL;

    // maintenance_clear_request->project
    cJSON *project = cJSON_GetObjectItemCaseSensitive(maintenance_clear_requestJSON, "project");
    if (cJSON_IsNull(project)) {
        project = NULL;
    }
    if (!project) {
        goto end;
    }

    
    if(!cJSON_IsString(project))
    {
    goto end; //String
    }


    if (project && !cJSON_IsNull(project)) project_local_str = strdup(project->valuestring);

    maintenance_clear_request_local_var = maintenance_clear_request_create_internal (
        project_local_str
        );

    if (!maintenance_clear_request_local_var) {
        goto end;
    }

    return maintenance_clear_request_local_var;
end:
    if (project_local_str) {
        free(project_local_str);
        project_local_str = NULL;
    }
    return NULL;

}
