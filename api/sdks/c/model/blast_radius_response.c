#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "blast_radius_response.h"



static blast_radius_response_t *blast_radius_response_create_internal(
    char *file,
    list_t *dependents,
    int *dependent_count,
    list_t *dependencies,
    int *dependency_count
    ) {
    blast_radius_response_t *blast_radius_response_local_var = malloc(sizeof(blast_radius_response_t));
    if (!blast_radius_response_local_var) {
        return NULL;
    }
    memset(blast_radius_response_local_var, 0, sizeof(blast_radius_response_t));
    blast_radius_response_local_var->_library_owned = 1;
    blast_radius_response_local_var->file = file;
    blast_radius_response_local_var->dependents = dependents;
    blast_radius_response_local_var->dependent_count = dependent_count;
    blast_radius_response_local_var->dependencies = dependencies;
    blast_radius_response_local_var->dependency_count = dependency_count;
    return blast_radius_response_local_var;
}

__attribute__((deprecated)) blast_radius_response_t *blast_radius_response_create(
    char *file,
    list_t *dependents,
    int *dependent_count,
    list_t *dependencies,
    int *dependency_count
    ) {
    int *dependent_count_copy = NULL;
    if (dependent_count) {
        dependent_count_copy = malloc(sizeof(int));
        if (dependent_count_copy) *dependent_count_copy = *dependent_count;
    }
    int *dependency_count_copy = NULL;
    if (dependency_count) {
        dependency_count_copy = malloc(sizeof(int));
        if (dependency_count_copy) *dependency_count_copy = *dependency_count;
    }
    blast_radius_response_t *result = blast_radius_response_create_internal (
        file,
        dependents,
        dependent_count_copy,
        dependencies,
        dependency_count_copy
        );
    if (!result) {
        free(dependent_count_copy);
        free(dependency_count_copy);
    }
    return result;
}

void blast_radius_response_free(blast_radius_response_t *blast_radius_response) {
    if(NULL == blast_radius_response){
        return ;
    }
    if(blast_radius_response->_library_owned != 1){
        fprintf(stderr, "WARNING: %s() does NOT free objects allocated by the user\n", "blast_radius_response_free");
        return ;
    }
    listEntry_t *listEntry;
    if (blast_radius_response->file) {
        free(blast_radius_response->file);
        blast_radius_response->file = NULL;
    }
    if (blast_radius_response->dependents) {
        list_ForEach(listEntry, blast_radius_response->dependents) {
            free(listEntry->data);
        }
        list_freeList(blast_radius_response->dependents);
        blast_radius_response->dependents = NULL;
    }
    if (blast_radius_response->dependent_count) {
        free(blast_radius_response->dependent_count);
        blast_radius_response->dependent_count = NULL;
    }
    if (blast_radius_response->dependencies) {
        list_ForEach(listEntry, blast_radius_response->dependencies) {
            free(listEntry->data);
        }
        list_freeList(blast_radius_response->dependencies);
        blast_radius_response->dependencies = NULL;
    }
    if (blast_radius_response->dependency_count) {
        free(blast_radius_response->dependency_count);
        blast_radius_response->dependency_count = NULL;
    }
    free(blast_radius_response);
}

cJSON *blast_radius_response_convertToJSON(blast_radius_response_t *blast_radius_response) {
    cJSON *item = cJSON_CreateObject();

    // blast_radius_response->file
    if(blast_radius_response->file) {
    if(cJSON_AddStringToObject(item, "file", blast_radius_response->file) == NULL) {
    goto fail; //String
    }
    }


    // blast_radius_response->dependents
    if(blast_radius_response->dependents) {
    cJSON *dependents = cJSON_AddArrayToObject(item, "dependents");
    if(dependents == NULL) {
        goto fail; //primitive container
    }

    listEntry_t *dependentsListEntry;
    list_ForEach(dependentsListEntry, blast_radius_response->dependents) {
    if(cJSON_AddStringToObject(dependents, "", dependentsListEntry->data) == NULL)
    {
        goto fail;
    }
    }
    }


    // blast_radius_response->dependent_count
    if(blast_radius_response->dependent_count) {
    if(cJSON_AddNumberToObject(item, "dependent_count", *blast_radius_response->dependent_count) == NULL) {
    goto fail; //Numeric
    }
    }


    // blast_radius_response->dependencies
    if(blast_radius_response->dependencies) {
    cJSON *dependencies = cJSON_AddArrayToObject(item, "dependencies");
    if(dependencies == NULL) {
        goto fail; //primitive container
    }

    listEntry_t *dependenciesListEntry;
    list_ForEach(dependenciesListEntry, blast_radius_response->dependencies) {
    if(cJSON_AddStringToObject(dependencies, "", dependenciesListEntry->data) == NULL)
    {
        goto fail;
    }
    }
    }


    // blast_radius_response->dependency_count
    if(blast_radius_response->dependency_count) {
    if(cJSON_AddNumberToObject(item, "dependency_count", *blast_radius_response->dependency_count) == NULL) {
    goto fail; //Numeric
    }
    }

    return item;
fail:
    if (item) {
        cJSON_Delete(item);
    }
    return NULL;
}

blast_radius_response_t *blast_radius_response_parseFromJSON(cJSON *blast_radius_responseJSON){

    blast_radius_response_t *blast_radius_response_local_var = NULL;

    char *file_local_str = NULL;

    // define the local list for blast_radius_response->dependents
    list_t *dependentsList = NULL;

    // define the local variable for blast_radius_response->dependent_count
    int *dependent_count_local_var = NULL;

    // define the local list for blast_radius_response->dependencies
    list_t *dependenciesList = NULL;

    // define the local variable for blast_radius_response->dependency_count
    int *dependency_count_local_var = NULL;

    // blast_radius_response->file
    cJSON *file = cJSON_GetObjectItemCaseSensitive(blast_radius_responseJSON, "file");
    if (cJSON_IsNull(file)) {
        file = NULL;
    }
    if (file) { 
    if(!cJSON_IsString(file) && !cJSON_IsNull(file))
    {
    goto end; //String
    }
    }

    // blast_radius_response->dependents
    cJSON *dependents = cJSON_GetObjectItemCaseSensitive(blast_radius_responseJSON, "dependents");
    if (cJSON_IsNull(dependents)) {
        dependents = NULL;
    }
    if (dependents) { 
    cJSON *dependents_local = NULL;
    if(!cJSON_IsArray(dependents)) {
        goto end;//primitive container
    }
    dependentsList = list_createList();

    cJSON_ArrayForEach(dependents_local, dependents)
    {
        if(!cJSON_IsString(dependents_local))
        {
            goto end;
        }
        list_addElement(dependentsList , strdup(dependents_local->valuestring));
    }
    }

    // blast_radius_response->dependent_count
    cJSON *dependent_count = cJSON_GetObjectItemCaseSensitive(blast_radius_responseJSON, "dependent_count");
    if (cJSON_IsNull(dependent_count)) {
        dependent_count = NULL;
    }
    if (dependent_count) { 
    if(!cJSON_IsNumber(dependent_count))
    {
    goto end; //Numeric
    }
    dependent_count_local_var = malloc(sizeof(int));
    if(!dependent_count_local_var)
    {
        goto end;
    }
    *dependent_count_local_var = dependent_count->valuedouble;
    }

    // blast_radius_response->dependencies
    cJSON *dependencies = cJSON_GetObjectItemCaseSensitive(blast_radius_responseJSON, "dependencies");
    if (cJSON_IsNull(dependencies)) {
        dependencies = NULL;
    }
    if (dependencies) { 
    cJSON *dependencies_local = NULL;
    if(!cJSON_IsArray(dependencies)) {
        goto end;//primitive container
    }
    dependenciesList = list_createList();

    cJSON_ArrayForEach(dependencies_local, dependencies)
    {
        if(!cJSON_IsString(dependencies_local))
        {
            goto end;
        }
        list_addElement(dependenciesList , strdup(dependencies_local->valuestring));
    }
    }

    // blast_radius_response->dependency_count
    cJSON *dependency_count = cJSON_GetObjectItemCaseSensitive(blast_radius_responseJSON, "dependency_count");
    if (cJSON_IsNull(dependency_count)) {
        dependency_count = NULL;
    }
    if (dependency_count) { 
    if(!cJSON_IsNumber(dependency_count))
    {
    goto end; //Numeric
    }
    dependency_count_local_var = malloc(sizeof(int));
    if(!dependency_count_local_var)
    {
        goto end;
    }
    *dependency_count_local_var = dependency_count->valuedouble;
    }


    if (file && !cJSON_IsNull(file)) file_local_str = strdup(file->valuestring);

    blast_radius_response_local_var = blast_radius_response_create_internal (
        file_local_str,
        dependents ? dependentsList : NULL,
        dependent_count_local_var,
        dependencies ? dependenciesList : NULL,
        dependency_count_local_var
        );

    if (!blast_radius_response_local_var) {
        goto end;
    }

    return blast_radius_response_local_var;
end:
    if (file_local_str) {
        free(file_local_str);
        file_local_str = NULL;
    }
    if (dependentsList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, dependentsList) {
            free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(dependentsList);
        dependentsList = NULL;
    }
    if (dependent_count_local_var) {
        free(dependent_count_local_var);
        dependent_count_local_var = NULL;
    }
    if (dependenciesList) {
        listEntry_t *listEntry = NULL;
        list_ForEach(listEntry, dependenciesList) {
            free(listEntry->data);
            listEntry->data = NULL;
        }
        list_freeList(dependenciesList);
        dependenciesList = NULL;
    }
    if (dependency_count_local_var) {
        free(dependency_count_local_var);
        dependency_count_local_var = NULL;
    }
    return NULL;

}
