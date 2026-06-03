#ifndef code_search_response_TEST
#define code_search_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_search_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_search_response.h"
code_search_response_t* instantiate_code_search_response(int include_optional);



code_search_response_t* instantiate_code_search_response(int include_optional) {
  code_search_response_t* code_search_response = NULL;
  if (include_optional) {
    code_search_response = code_search_response_create(
      "0",
      list_createList(),
      "0"
    );
  } else {
    code_search_response = code_search_response_create(
      "0",
      list_createList(),
      "0"
    );
  }

  return code_search_response;
}


#ifdef code_search_response_MAIN

void test_code_search_response(int include_optional) {
    code_search_response_t* code_search_response_1 = instantiate_code_search_response(include_optional);

	cJSON* jsoncode_search_response_1 = code_search_response_convertToJSON(code_search_response_1);
	printf("code_search_response :\n%s\n", cJSON_Print(jsoncode_search_response_1));
	code_search_response_t* code_search_response_2 = code_search_response_parseFromJSON(jsoncode_search_response_1);
	cJSON* jsoncode_search_response_2 = code_search_response_convertToJSON(code_search_response_2);
	printf("repeating code_search_response:\n%s\n", cJSON_Print(jsoncode_search_response_2));
}

int main() {
  test_code_search_response(1);
  test_code_search_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_search_response_MAIN
#endif // code_search_response_TEST
