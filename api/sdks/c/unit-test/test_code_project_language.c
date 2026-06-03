#ifndef code_project_language_TEST
#define code_project_language_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_project_language_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_project_language.h"
code_project_language_t* instantiate_code_project_language(int include_optional);



code_project_language_t* instantiate_code_project_language(int include_optional) {
  code_project_language_t* code_project_language = NULL;
  if (include_optional) {
    code_project_language = code_project_language_create(
      "0",
      56
    );
  } else {
    code_project_language = code_project_language_create(
      "0",
      56
    );
  }

  return code_project_language;
}


#ifdef code_project_language_MAIN

void test_code_project_language(int include_optional) {
    code_project_language_t* code_project_language_1 = instantiate_code_project_language(include_optional);

	cJSON* jsoncode_project_language_1 = code_project_language_convertToJSON(code_project_language_1);
	printf("code_project_language :\n%s\n", cJSON_Print(jsoncode_project_language_1));
	code_project_language_t* code_project_language_2 = code_project_language_parseFromJSON(jsoncode_project_language_1);
	cJSON* jsoncode_project_language_2 = code_project_language_convertToJSON(code_project_language_2);
	printf("repeating code_project_language:\n%s\n", cJSON_Print(jsoncode_project_language_2));
}

int main() {
  test_code_project_language(1);
  test_code_project_language(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_project_language_MAIN
#endif // code_project_language_TEST
