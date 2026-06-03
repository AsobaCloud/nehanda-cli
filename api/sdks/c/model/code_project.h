/*
 * code_project.h
 *
 * 
 */

#ifndef _code_project_H_
#define _code_project_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct code_project_t code_project_t;




typedef struct code_project_t {
    char *name; // string
    char *root; // string
    char *scanned_at; // string

    int _library_owned; // Is the library responsible for freeing this object?
} code_project_t;

__attribute__((deprecated)) code_project_t *code_project_create(
    char *name,
    char *root,
    char *scanned_at
);

void code_project_free(code_project_t *code_project);

code_project_t *code_project_parseFromJSON(cJSON *code_projectJSON);

cJSON *code_project_convertToJSON(code_project_t *code_project);

#endif /* _code_project_H_ */

