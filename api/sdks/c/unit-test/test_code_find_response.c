#ifndef code_find_response_TEST
#define code_find_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_find_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_find_response.h"
code_find_response_t* instantiate_code_find_response(int include_optional);



code_find_response_t* instantiate_code_find_response(int include_optional) {
  code_find_response_t* code_find_response = NULL;
  if (include_optional) {
    code_find_response = code_find_response_create(
      list_createList(),
      "0"
    );
  } else {
    code_find_response = code_find_response_create(
      list_createList(),
      "0"
    );
  }

  return code_find_response;
}


#ifdef code_find_response_MAIN

void test_code_find_response(int include_optional) {
    code_find_response_t* code_find_response_1 = instantiate_code_find_response(include_optional);

	cJSON* jsoncode_find_response_1 = code_find_response_convertToJSON(code_find_response_1);
	printf("code_find_response :\n%s\n", cJSON_Print(jsoncode_find_response_1));
	code_find_response_t* code_find_response_2 = code_find_response_parseFromJSON(jsoncode_find_response_1);
	cJSON* jsoncode_find_response_2 = code_find_response_convertToJSON(code_find_response_2);
	printf("repeating code_find_response:\n%s\n", cJSON_Print(jsoncode_find_response_2));
}

int main() {
  test_code_find_response(1);
  test_code_find_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_find_response_MAIN
#endif // code_find_response_TEST
