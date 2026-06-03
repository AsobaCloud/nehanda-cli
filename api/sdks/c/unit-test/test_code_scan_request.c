#ifndef code_scan_request_TEST
#define code_scan_request_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_scan_request_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_scan_request.h"
code_scan_request_t* instantiate_code_scan_request(int include_optional);



code_scan_request_t* instantiate_code_scan_request(int include_optional) {
  code_scan_request_t* code_scan_request = NULL;
  if (include_optional) {
    code_scan_request = code_scan_request_create(
      "0",
      "0",
      1
    );
  } else {
    code_scan_request = code_scan_request_create(
      "0",
      "0",
      1
    );
  }

  return code_scan_request;
}


#ifdef code_scan_request_MAIN

void test_code_scan_request(int include_optional) {
    code_scan_request_t* code_scan_request_1 = instantiate_code_scan_request(include_optional);

	cJSON* jsoncode_scan_request_1 = code_scan_request_convertToJSON(code_scan_request_1);
	printf("code_scan_request :\n%s\n", cJSON_Print(jsoncode_scan_request_1));
	code_scan_request_t* code_scan_request_2 = code_scan_request_parseFromJSON(jsoncode_scan_request_1);
	cJSON* jsoncode_scan_request_2 = code_scan_request_convertToJSON(code_scan_request_2);
	printf("repeating code_scan_request:\n%s\n", cJSON_Print(jsoncode_scan_request_2));
}

int main() {
  test_code_scan_request(1);
  test_code_scan_request(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_scan_request_MAIN
#endif // code_scan_request_TEST
