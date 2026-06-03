#ifndef code_structure_response_TEST
#define code_structure_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_structure_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_structure_response.h"
code_structure_response_t* instantiate_code_structure_response(int include_optional);



code_structure_response_t* instantiate_code_structure_response(int include_optional) {
  code_structure_response_t* code_structure_response = NULL;
  if (include_optional) {
    code_structure_response = code_structure_response_create(
      "0",
      list_createList()
    );
  } else {
    code_structure_response = code_structure_response_create(
      "0",
      list_createList()
    );
  }

  return code_structure_response;
}


#ifdef code_structure_response_MAIN

void test_code_structure_response(int include_optional) {
    code_structure_response_t* code_structure_response_1 = instantiate_code_structure_response(include_optional);

	cJSON* jsoncode_structure_response_1 = code_structure_response_convertToJSON(code_structure_response_1);
	printf("code_structure_response :\n%s\n", cJSON_Print(jsoncode_structure_response_1));
	code_structure_response_t* code_structure_response_2 = code_structure_response_parseFromJSON(jsoncode_structure_response_1);
	cJSON* jsoncode_structure_response_2 = code_structure_response_convertToJSON(code_structure_response_2);
	printf("repeating code_structure_response:\n%s\n", cJSON_Print(jsoncode_structure_response_2));
}

int main() {
  test_code_structure_response(1);
  test_code_structure_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_structure_response_MAIN
#endif // code_structure_response_TEST
