#ifndef code_find_hit_TEST
#define code_find_hit_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_find_hit_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_find_hit.h"
code_find_hit_t* instantiate_code_find_hit(int include_optional);



code_find_hit_t* instantiate_code_find_hit(int include_optional) {
  code_find_hit_t* code_find_hit = NULL;
  if (include_optional) {
    code_find_hit = code_find_hit_create(
      "0",
      "0",
      56,
      "0"
    );
  } else {
    code_find_hit = code_find_hit_create(
      "0",
      "0",
      56,
      "0"
    );
  }

  return code_find_hit;
}


#ifdef code_find_hit_MAIN

void test_code_find_hit(int include_optional) {
    code_find_hit_t* code_find_hit_1 = instantiate_code_find_hit(include_optional);

	cJSON* jsoncode_find_hit_1 = code_find_hit_convertToJSON(code_find_hit_1);
	printf("code_find_hit :\n%s\n", cJSON_Print(jsoncode_find_hit_1));
	code_find_hit_t* code_find_hit_2 = code_find_hit_parseFromJSON(jsoncode_find_hit_1);
	cJSON* jsoncode_find_hit_2 = code_find_hit_convertToJSON(code_find_hit_2);
	printf("repeating code_find_hit:\n%s\n", cJSON_Print(jsoncode_find_hit_2));
}

int main() {
  test_code_find_hit(1);
  test_code_find_hit(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_find_hit_MAIN
#endif // code_find_hit_TEST
