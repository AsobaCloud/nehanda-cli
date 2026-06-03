#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "code_projects_response.h"



static code_projects_response_t *code_projects_response_create_internal(
    char *status,
    list_t *projects,
    char *next_cursor
    ) {
    code_projects_response_t *code_projects_response_local_var = malloc(sizeof(code_projects_response_t));
    if (!code_projects_response_local_var) {
        return NULL;
    }
    memset(code_projects_response_local_var, 0, sizeof(code_projects_response_t));
    code_projects_response_local_var->_library_owned = 1;
    code_projects_response_local_var->status = status;
    code_projects_response_local_var->projects = projects;
    code_projects_response_local_var->next_cursor = next_cursor;
    return code_projects_response_local_var;
}

__attribute__((deprecated)) code_projects_response_t *code_projects_response_create(
    char *status,
    list_t *projects,
    char *next_cursor
    ) {
    code_projects_response_t *result = code_projects_response_create_internal (
        status,
        projects,
        next_cursor
        );
    if (!result) {
    }
    return result;
}

void code_projects_response_free(code_projects_response_t *code_projects_response) {
    if(NULL == code_projects_response){
        return ;
    }
    if(code_projects_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "code_projects_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (code_projects_response->status) {
        free(code_projects_response->status);
        code_projects_response->status = NULL;
    }
    if (code_projects_response->projects) {
        list_ForEach(listEntry, code_projects_response->projects) {
            code_project_free(listEntry->data);
        }
        list_freeList(code_projects_response->projects);
        code_projects_response->projects = NULL;
    }
    if (code_projects_response->next_cursor) {
        free(code_projects_response->next_cursor);
        code_projects_response->next_cursor = NULL;
    }
    free(code_projects_response);
}

cJSON *code_projects_response_convertToJSON(code_projects_response_t *code_projects_response) {
    cJSON *item = cJSON_CreateObject();

    // code_projects_response->status
    if(code_projects_response->status) {
    if(cJSON_AddStringToObject(item, "status", code_projects_response->status) == NULL) {
    goto fail; //String
    }
    }


    // code_projects_response->projects
    if(code_projects_response->projects) {
    cJSON *projects = cJSON_AddArrayToObject(item, "projects");
    if(projects == NULL) {
    goto fail; //nonprimitive container
    }

    listEntry_t *projectsListEntry;
    if (code_projects_response->projects) {
    list_ForEach(projectsListEntry, code_projects_response->projects) {
    cJSON *itemLocal = code_project_convertToJSON(projectsListEntry->data);
    if(itemLocal == NULL) {
    goto fail;
    }
    cJSON_AddItemToArray(projects, itemLocal);
    }
    }
    }


    // code_projects_response->next_cursor
    if(code_projects_response->next_cursor) {
    if(cJSON_AddStringToObject(item, "next_cursor", code_projects_response->next_cursor) == NULL) {
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

code_projects_response_t *code_projects_response_parseFromJSON(cJSON *code_projects_responseJSON){

    code_projects_response_t *code_projects_response_local_var = NULL;

    char *status_local_str = NULL;

    // define the local list for code_projects_response->projects
    list_t *projectsList = NULL;

    char *next_cursor_local_str = NULL;

    // code_projects_response->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(code_projects_responseJSON, "status");
    if (cJSON_IsNull(status)) {
        status = NULL;
    }
    if (status) { 
    if(!cJSON_IsString(status) && !cJSON_IsNull(status))
    {
    goto end; //String
    }
    }

    // code_projects_response->projects
    cJSON *projects = cJSON_GetObjectItemCaseSensitive(code_projects_responseJSON, "projects");
    if (cJSON_IsNull(projects)) {
        projects = NULL;
    }
    if (projects) { 
    cJSON *projects_local_nonprimitive = NULL;
    if(!cJSON_IsArray(projects)){
        goto end; //nonprimitive container
    }

    projectsList = list_createList();

    cJSON_ArrayForEach(projects_local_nonprimitive,projects )
    {
        if(!cJSON_IsObject(projects_local_nonprimitive)){
            goto end;
        }
        code_project_t *projectsItem = code_project_parseFromJSON(projects_local_nonprimitive);

        list_addElement(projectsList, projectsItem);
    }
    }

    // code_projects_response->next_cursor
    cJSON *next_cursor = cJSON_GetObjectItemCaseSensitive(code_projects_responseJSON, "next_cursor");
    if (cJSON_IsNull(next_cursor)) {
        next_cursor = NULL;
    }
    if (next_cursor) { 
    if(!cJSON_IsString(next_cursor) && !cJSON_IsNull(next_cursor))
    {
    goto end; //String
    }
    }


    if (status && !cJSON_IsNull(status)) status_local_str = strdup(status->valuestring);
    if (next_cursor && !cJSON_IsNull(next_cursor)) next_cursor_local_str = strdup(next_cursor->valuestring);

    code_projects_response_local_var = code_projects_response_create_internal (
        status_local_str,
        projects ? projectsList : NULL,
        next_cursor_local_str
        );

    if (!code_projects_response_local_var) {
        goto end;
    }

    return code_projects_response_local_var;
end:
    if (status_local_str) {
        free(status_local_str);
        status_local_str = NULL;
    }
    if (projectsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, projectsList) {
            code_project_free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(projectsList);
        projectsList = NULL;
    }
    if (next_cursor_local_str) {
        free(next_cursor_local_str);
        next_cursor_local_str = NULL;
    }
    return NULL;

}
