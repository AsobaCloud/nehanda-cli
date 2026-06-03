#ifndef code_scan_response_TEST
#define code_scan_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_scan_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_scan_response.h"
code_scan_response_t* instantiate_code_scan_response(int include_optional);



code_scan_response_t* instantiate_code_scan_response(int include_optional) {
  code_scan_response_t* code_scan_response = NULL;
  if (include_optional) {
    code_scan_response = code_scan_response_create(
      "ok",
      1,
      "0",
      56,
      56
    );
  } else {
    code_scan_response = code_scan_response_create(
      "ok",
      1,
      "0",
      56,
      56
    );
  }

  return code_scan_response;
}


#ifdef code_scan_response_MAIN

void test_code_scan_response(int include_optional) {
    code_scan_response_t* code_scan_response_1 = instantiate_code_scan_response(include_optional);

	cJSON* jsoncode_scan_response_1 = code_scan_response_convertToJSON(code_scan_response_1);
	printf("code_scan_response :\n%s\n", cJSON_Print(jsoncode_scan_response_1));
	code_scan_response_t* code_scan_response_2 = code_scan_response_parseFromJSON(jsoncode_scan_response_1);
	cJSON* jsoncode_scan_response_2 = code_scan_response_convertToJSON(code_scan_response_2);
	printf("repeating code_scan_response:\n%s\n", cJSON_Print(jsoncode_scan_response_2));
}

int main() {
  test_code_scan_response(1);
  test_code_scan_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_scan_response_MAIN
#endif // code_scan_response_TEST
