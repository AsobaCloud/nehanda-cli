#ifndef code_build_response_TEST
#define code_build_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_build_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_build_response.h"
code_build_response_t* instantiate_code_build_response(int include_optional);



code_build_response_t* instantiate_code_build_response(int include_optional) {
  code_build_response_t* code_build_response = NULL;
  if (include_optional) {
    code_build_response = code_build_response_create(
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
    code_build_response = code_build_response_create(
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

  return code_build_response;
}


#ifdef code_build_response_MAIN

void test_code_build_response(int include_optional) {
    code_build_response_t* code_build_response_1 = instantiate_code_build_response(include_optional);

	cJSON* jsoncode_build_response_1 = code_build_response_convertToJSON(code_build_response_1);
	printf("code_build_response :\n%s\n", cJSON_Print(jsoncode_build_response_1));
	code_build_response_t* code_build_response_2 = code_build_response_parseFromJSON(jsoncode_build_response_1);
	cJSON* jsoncode_build_response_2 = code_build_response_convertToJSON(code_build_response_2);
	printf("repeating code_build_response:\n%s\n", cJSON_Print(jsoncode_build_response_2));
}

int main() {
  test_code_build_response(1);
  test_code_build_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_build_response_MAIN
#endif // code_build_response_TEST
