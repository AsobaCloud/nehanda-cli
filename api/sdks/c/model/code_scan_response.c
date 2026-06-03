#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "code_scan_response.h"



static code_scan_response_t *code_scan_response_create_internal(
    char *status,
    int *skipped,
    char *project,
    int *files,
    int *inspected
    ) {
    code_scan_response_t *code_scan_response_local_var = malloc(sizeof(code_scan_response_t));
    if (!code_scan_response_local_var) {
        return NULL;
    }
    memset(code_scan_response_local_var, 0, sizeof(code_scan_response_t));
    code_scan_response_local_var->_library_owned = 1;
    code_scan_response_local_var->status = status;
    code_scan_response_local_var->skipped = skipped;
    code_scan_response_local_var->project = project;
    code_scan_response_local_var->files = files;
    code_scan_response_local_var->inspected = inspected;
    return code_scan_response_local_var;
}

__attribute__((deprecated)) code_scan_response_t *code_scan_response_create(
    char *status,
    int *skipped,
    char *project,
    int *files,
    int *inspected
    ) {
    int *skipped_copy = NULL;
    if (skipped) {
        skipped_copy = malloc(sizeof(int));
        if (skipped_copy) *skipped_copy = *skipped;
    }
    int *files_copy = NULL;
    if (files) {
        files_copy = malloc(sizeof(int));
        if (files_copy) *files_copy = *files;
    }
    int *inspected_copy = NULL;
    if (inspected) {
        inspected_copy = malloc(sizeof(int));
        if (inspected_copy) *inspected_copy = *inspected;
    }
    code_scan_response_t *result = code_scan_response_create_internal (
        status,
        skipped_copy,
        project,
        files_copy,
        inspected_copy
        );
    if (!result) {
        free(skipped_copy);
        free(files_copy);
        free(inspected_copy);
    }
    return result;
}

void code_scan_response_free(code_scan_response_t *code_scan_response) {
    if(NULL == code_scan_response){
        return ;
    }
    if(code_scan_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "code_scan_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (code_scan_response->status) {
        free(code_scan_response->status);
        code_scan_response->status = NULL;
    }
    if (code_scan_response->skipped) {
        free(code_scan_response->skipped);
        code_scan_response->skipped = NULL;
    }
    if (code_scan_response->project) {
        free(code_scan_response->project);
        code_scan_response->project = NULL;
    }
    if (code_scan_response->files) {
        free(code_scan_response->files);
        code_scan_response->files = NULL;
    }
    if (code_scan_response->inspected) {
        free(code_scan_response->inspected);
        code_scan_response->inspected = NULL;
    }
    free(code_scan_response);
}

cJSON *code_scan_response_convertToJSON(code_scan_response_t *code_scan_response) {
    cJSON *item = cJSON_CreateObject();

    // code_scan_response->status
    if (!code_scan_response->status) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "status", code_scan_response->status) == NULL) {
    goto fail; //String
    }


    // code_scan_response->skipped
    if (!code_scan_response->skipped) {
        goto fail;
    }
    if(cJSON_AddBoolToObject(item, "skipped", *code_scan_response->skipped) == NULL) {
    goto fail; //Bool
    }


    // code_scan_response->project
    if (!code_scan_response->project) {
        goto fail;
    }
    if(cJSON_AddStringToObject(item, "project", code_scan_response->project) == NULL) {
    goto fail; //String
    }


    // code_scan_response->files
    if (!code_scan_response->files) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "files", *code_scan_response->files) == NULL) {
    goto fail; //Numeric
    }


    // code_scan_response->inspected
    if (!code_scan_response->inspected) {
        goto fail;
    }
    if(cJSON_AddNumberToObject(item, "inspected", *code_scan_response->inspected) == NULL) {
    goto fail; //Numeric
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

code_scan_response_t *code_scan_response_parseFromJSON(cJSON *code_scan_responseJSON){

    code_scan_response_t *code_scan_response_local_var = NULL;

    char *status_local_str = NULL;

    // define the local variable for code_scan_response->skipped
    int *skipped_local_var = NULL;

    char *project_local_str = NULL;

    // define the local variable for code_scan_response->files
    int *files_local_var = NULL;

    // define the local variable for code_scan_response->inspected
    int *inspected_local_var = NULL;

    // code_scan_response->status
    cJSON *status = cJSON_GetObjectItemCaseSensitive(code_scan_responseJSON, "status");
    if (cJSON_IsNull(status)) {
        status = NULL;
    }
    if (!status) {
        goto end;
    }

    
    if(!cJSON_IsString(status))
    {
    goto end; //String
    }

    // code_scan_response->skipped
    cJSON *skipped = cJSON_GetObjectItemCaseSensitive(code_scan_responseJSON, "skipped");
    if (cJSON_IsNull(skipped)) {
        skipped = NULL;
    }
    if (!skipped) {
        goto end;
    }

    
    if(!cJSON_IsBool(skipped))
    {
    goto end; //Bool
    }
    skipped_local_var = malloc(sizeof(int));
    if(!skipped_local_var)
    {
        goto end;
    }
    *skipped_local_var = skipped->valueint;

    // code_scan_response->project
    cJSON *project = cJSON_GetObjectItemCaseSensitive(code_scan_responseJSON, "project");
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

    // code_scan_response->files
    cJSON *files = cJSON_GetObjectItemCaseSensitive(code_scan_responseJSON, "files");
    if (cJSON_IsNull(files)) {
        files = NULL;
    }
    if (!files) {
        goto end;
    }

    
    if(!cJSON_IsNumber(files))
    {
    goto end; //Numeric
    }
    files_local_var = malloc(sizeof(int));
    if(!files_local_var)
    {
        goto end;
    }
    *files_local_var = files->valuedouble;

    // code_scan_response->inspected
    cJSON *inspected = cJSON_GetObjectItemCaseSensitive(code_scan_responseJSON, "inspected");
    if (cJSON_IsNull(inspected)) {
        inspected = NULL;
    }
    if (!inspected) {
        goto end;
    }

    
    if(!cJSON_IsNumber(inspected))
    {
    goto end; //Numeric
    }
    inspected_local_var = malloc(sizeof(int));
    if(!inspected_local_var)
    {
        goto end;
    }
    *inspected_local_var = inspected->valuedouble;


    if (status && !cJSON_IsNull(status)) status_local_str = strdup(status->valuestring);
    if (project && !cJSON_IsNull(project)) project_local_str = strdup(project->valuestring);

    code_scan_response_local_var = code_scan_response_create_internal (
        status_local_str,
        skipped_local_var,
        project_local_str,
        files_local_var,
        inspected_local_var
        );

    if (!code_scan_response_local_var) {
        goto end;
    }

    return code_scan_response_local_var;
end:
    if (status_local_str) {
        free(status_local_str);
        status_local_str = NULL;
    }
    if (skipped_local_var) {
        free(skipped_local_var);
        skipped_local_var = NULL;
    }
    if (project_local_str) {
        free(project_local_str);
        project_local_str = NULL;
    }
    if (files_local_var) {
        free(files_local_var);
        files_local_var = NULL;
    }
    if (inspected_local_var) {
        free(inspected_local_var);
        inspected_local_var = NULL;
    }
    return NULL;

}
