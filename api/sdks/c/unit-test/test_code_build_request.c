#ifndef code_build_request_TEST
#define code_build_request_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_build_request_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_build_request.h"
code_build_request_t* instantiate_code_build_request(int include_optional);



code_build_request_t* instantiate_code_build_request(int include_optional) {
  code_build_request_t* code_build_request = NULL;
  if (include_optional) {
    code_build_request = code_build_request_create(
      "0",
      "0",
      "0",
      1
    );
  } else {
    code_build_request = code_build_request_create(
      "0",
      "0",
      "0",
      1
    );
  }

  return code_build_request;
}


#ifdef code_build_request_MAIN

void test_code_build_request(int include_optional) {
    code_build_request_t* code_build_request_1 = instantiate_code_build_request(include_optional);

	cJSON* jsoncode_build_request_1 = code_build_request_convertToJSON(code_build_request_1);
	printf("code_build_request :\n%s\n", cJSON_Print(jsoncode_build_request_1));
	code_build_request_t* code_build_request_2 = code_build_request_parseFromJSON(jsoncode_build_request_1);
	cJSON* jsoncode_build_request_2 = code_build_request_convertToJSON(code_build_request_2);
	printf("repeating code_build_request:\n%s\n", cJSON_Print(jsoncode_build_request_2));
}

int main() {
  test_code_build_request(1);
  test_code_build_request(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_build_request_MAIN
#endif // code_build_request_TEST
