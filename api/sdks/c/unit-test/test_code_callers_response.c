#ifndef code_callers_response_TEST
#define code_callers_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_callers_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_callers_response.h"
code_callers_response_t* instantiate_code_callers_response(int include_optional);



code_callers_response_t* instantiate_code_callers_response(int include_optional) {
  code_callers_response_t* code_callers_response = NULL;
  if (include_optional) {
    code_callers_response = code_callers_response_create(
      "0",
      list_createList(),
      "0"
    );
  } else {
    code_callers_response = code_callers_response_create(
      "0",
      list_createList(),
      "0"
    );
  }

  return code_callers_response;
}


#ifdef code_callers_response_MAIN

void test_code_callers_response(int include_optional) {
    code_callers_response_t* code_callers_response_1 = instantiate_code_callers_response(include_optional);

	cJSON* jsoncode_callers_response_1 = code_callers_response_convertToJSON(code_callers_response_1);
	printf("code_callers_response :\n%s\n", cJSON_Print(jsoncode_callers_response_1));
	code_callers_response_t* code_callers_response_2 = code_callers_response_parseFromJSON(jsoncode_callers_response_1);
	cJSON* jsoncode_callers_response_2 = code_callers_response_convertToJSON(code_callers_response_2);
	printf("repeating code_callers_response:\n%s\n", cJSON_Print(jsoncode_callers_response_2));
}

int main() {
  test_code_callers_response(1);
  test_code_callers_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_callers_response_MAIN
#endif // code_callers_response_TEST
