#ifndef code_project_stats_response_TEST
#define code_project_stats_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_project_stats_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_project_stats_response.h"
code_project_stats_response_t* instantiate_code_project_stats_response(int include_optional);



code_project_stats_response_t* instantiate_code_project_stats_response(int include_optional) {
  code_project_stats_response_t* code_project_stats_response = NULL;
  if (include_optional) {
    code_project_stats_response = code_project_stats_response_create(
      "0",
      "0",
      56,
      56,
      list_createList()
    );
  } else {
    code_project_stats_response = code_project_stats_response_create(
      "0",
      "0",
      56,
      56,
      list_createList()
    );
  }

  return code_project_stats_response;
}


#ifdef code_project_stats_response_MAIN

void test_code_project_stats_response(int include_optional) {
    code_project_stats_response_t* code_project_stats_response_1 = instantiate_code_project_stats_response(include_optional);

	cJSON* jsoncode_project_stats_response_1 = code_project_stats_response_convertToJSON(code_project_stats_response_1);
	printf("code_project_stats_response :\n%s\n", cJSON_Print(jsoncode_project_stats_response_1));
	code_project_stats_response_t* code_project_stats_response_2 = code_project_stats_response_parseFromJSON(jsoncode_project_stats_response_1);
	cJSON* jsoncode_project_stats_response_2 = code_project_stats_response_convertToJSON(code_project_stats_response_2);
	printf("repeating code_project_stats_response:\n%s\n", cJSON_Print(jsoncode_project_stats_response_2));
}

int main() {
  test_code_project_stats_response(1);
  test_code_project_stats_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_project_stats_response_MAIN
#endif // code_project_stats_response_TEST
