#ifndef code_search_hit_TEST
#define code_search_hit_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_search_hit_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_search_hit.h"
code_search_hit_t* instantiate_code_search_hit(int include_optional);



code_search_hit_t* instantiate_code_search_hit(int include_optional) {
  code_search_hit_t* code_search_hit = NULL;
  if (include_optional) {
    code_search_hit = code_search_hit_create(
      "0",
      "0",
      "0",
      1.337
    );
  } else {
    code_search_hit = code_search_hit_create(
      "0",
      "0",
      "0",
      1.337
    );
  }

  return code_search_hit;
}


#ifdef code_search_hit_MAIN

void test_code_search_hit(int include_optional) {
    code_search_hit_t* code_search_hit_1 = instantiate_code_search_hit(include_optional);

	cJSON* jsoncode_search_hit_1 = code_search_hit_convertToJSON(code_search_hit_1);
	printf("code_search_hit :\n%s\n", cJSON_Print(jsoncode_search_hit_1));
	code_search_hit_t* code_search_hit_2 = code_search_hit_parseFromJSON(jsoncode_search_hit_1);
	cJSON* jsoncode_search_hit_2 = code_search_hit_convertToJSON(code_search_hit_2);
	printf("repeating code_search_hit:\n%s\n", cJSON_Print(jsoncode_search_hit_2));
}

int main() {
  test_code_search_hit(1);
  test_code_search_hit(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_search_hit_MAIN
#endif // code_search_hit_TEST
