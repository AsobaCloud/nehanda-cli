#ifndef code_caller_hit_TEST
#define code_caller_hit_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_caller_hit_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_caller_hit.h"
code_caller_hit_t* instantiate_code_caller_hit(int include_optional);



code_caller_hit_t* instantiate_code_caller_hit(int include_optional) {
  code_caller_hit_t* code_caller_hit = NULL;
  if (include_optional) {
    code_caller_hit = code_caller_hit_create(
      "0",
      "0",
      "0",
      56
    );
  } else {
    code_caller_hit = code_caller_hit_create(
      "0",
      "0",
      "0",
      56
    );
  }

  return code_caller_hit;
}


#ifdef code_caller_hit_MAIN

void test_code_caller_hit(int include_optional) {
    code_caller_hit_t* code_caller_hit_1 = instantiate_code_caller_hit(include_optional);

	cJSON* jsoncode_caller_hit_1 = code_caller_hit_convertToJSON(code_caller_hit_1);
	printf("code_caller_hit :\n%s\n", cJSON_Print(jsoncode_caller_hit_1));
	code_caller_hit_t* code_caller_hit_2 = code_caller_hit_parseFromJSON(jsoncode_caller_hit_1);
	cJSON* jsoncode_caller_hit_2 = code_caller_hit_convertToJSON(code_caller_hit_2);
	printf("repeating code_caller_hit:\n%s\n", cJSON_Print(jsoncode_caller_hit_2));
}

int main() {
  test_code_caller_hit(1);
  test_code_caller_hit(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_caller_hit_MAIN
#endif // code_caller_hit_TEST
