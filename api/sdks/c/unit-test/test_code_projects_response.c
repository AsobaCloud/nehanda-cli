#ifndef code_projects_response_TEST
#define code_projects_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_projects_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_projects_response.h"
code_projects_response_t* instantiate_code_projects_response(int include_optional);



code_projects_response_t* instantiate_code_projects_response(int include_optional) {
  code_projects_response_t* code_projects_response = NULL;
  if (include_optional) {
    code_projects_response = code_projects_response_create(
      "0",
      list_createList(),
      "0"
    );
  } else {
    code_projects_response = code_projects_response_create(
      "0",
      list_createList(),
      "0"
    );
  }

  return code_projects_response;
}


#ifdef code_projects_response_MAIN

void test_code_projects_response(int include_optional) {
    code_projects_response_t* code_projects_response_1 = instantiate_code_projects_response(include_optional);

	cJSON* jsoncode_projects_response_1 = code_projects_response_convertToJSON(code_projects_response_1);
	printf("code_projects_response :\n%s\n", cJSON_Print(jsoncode_projects_response_1));
	code_projects_response_t* code_projects_response_2 = code_projects_response_parseFromJSON(jsoncode_projects_response_1);
	cJSON* jsoncode_projects_response_2 = code_projects_response_convertToJSON(code_projects_response_2);
	printf("repeating code_projects_response:\n%s\n", cJSON_Print(jsoncode_projects_response_2));
}

int main() {
  test_code_projects_response(1);
  test_code_projects_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_projects_response_MAIN
#endif // code_projects_response_TEST
