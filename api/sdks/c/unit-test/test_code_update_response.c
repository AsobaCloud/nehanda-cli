#ifndef code_update_response_TEST
#define code_update_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_update_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_update_response.h"
code_update_response_t* instantiate_code_update_response(int include_optional);



code_update_response_t* instantiate_code_update_response(int include_optional) {
  code_update_response_t* code_update_response = NULL;
  if (include_optional) {
    code_update_response = code_update_response_create(
      "ok",
      "0",
      56,
      56,
      56,
      56,
      56,
      56,
      56
    );
  } else {
    code_update_response = code_update_response_create(
      "ok",
      "0",
      56,
      56,
      56,
      56,
      56,
      56,
      56
    );
  }

  return code_update_response;
}


#ifdef code_update_response_MAIN

void test_code_update_response(int include_optional) {
    code_update_response_t* code_update_response_1 = instantiate_code_update_response(include_optional);

	cJSON* jsoncode_update_response_1 = code_update_response_convertToJSON(code_update_response_1);
	printf("code_update_response :\n%s\n", cJSON_Print(jsoncode_update_response_1));
	code_update_response_t* code_update_response_2 = code_update_response_parseFromJSON(jsoncode_update_response_1);
	cJSON* jsoncode_update_response_2 = code_update_response_convertToJSON(code_update_response_2);
	printf("repeating code_update_response:\n%s\n", cJSON_Print(jsoncode_update_response_2));
}

int main() {
  test_code_update_response(1);
  test_code_update_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_update_response_MAIN
#endif // code_update_response_TEST
