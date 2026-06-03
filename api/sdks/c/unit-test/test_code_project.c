#ifndef code_project_TEST
#define code_project_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_project_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_project.h"
code_project_t* instantiate_code_project(int include_optional);



code_project_t* instantiate_code_project(int include_optional) {
  code_project_t* code_project = NULL;
  if (include_optional) {
    code_project = code_project_create(
      "0",
      "0",
      "0"
    );
  } else {
    code_project = code_project_create(
      "0",
      "0",
      "0"
    );
  }

  return code_project;
}


#ifdef code_project_MAIN

void test_code_project(int include_optional) {
    code_project_t* code_project_1 = instantiate_code_project(include_optional);

	cJSON* jsoncode_project_1 = code_project_convertToJSON(code_project_1);
	printf("code_project :\n%s\n", cJSON_Print(jsoncode_project_1));
	code_project_t* code_project_2 = code_project_parseFromJSON(jsoncode_project_1);
	cJSON* jsoncode_project_2 = code_project_convertToJSON(code_project_2);
	printf("repeating code_project:\n%s\n", cJSON_Print(jsoncode_project_2));
}

int main() {
  test_code_project(1);
  test_code_project(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_project_MAIN
#endif // code_project_TEST
