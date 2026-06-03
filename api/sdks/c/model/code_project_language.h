/*
 * code_project_language.h
 *
 * 
 */

#ifndef _code_project_language_H_
#define _code_project_language_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

typedef struct code_project_language_t code_project_language_t;




typedef struct code_project_language_t {
    char *lang; // string
    int *count; //numeric

    int _library_owned; // Is the library responsible for freeing this object?
} code_project_language_t;

__attribute__((deprecated)) code_project_language_t *code_project_language_create(
    char *lang,
    int *count
);

void code_project_language_free(code_project_language_t *code_project_language);

code_project_language_t *code_project_language_parseFromJSON(cJSON *code_project_languageJSON);

cJSON *code_project_language_convertToJSON(code_project_language_t *code_project_language);

#endif /* _code_project_language_H_ */

