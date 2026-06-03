#ifndef code_definition_TEST
#define code_definition_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define code_definition_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/code_definition.h"
code_definition_t* instantiate_code_definition(int include_optional);



code_definition_t* instantiate_code_definition(int include_optional) {
  code_definition_t* code_definition = NULL;
  if (include_optional) {
    code_definition = code_definition_create(
      "0",
      "0",
      56
    );
  } else {
    code_definition = code_definition_create(
      "0",
      "0",
      56
    );
  }

  return code_definition;
}


#ifdef code_definition_MAIN

void test_code_definition(int include_optional) {
    code_definition_t* code_definition_1 = instantiate_code_definition(include_optional);

	cJSON* jsoncode_definition_1 = code_definition_convertToJSON(code_definition_1);
	printf("code_definition :\n%s\n", cJSON_Print(jsoncode_definition_1));
	code_definition_t* code_definition_2 = code_definition_parseFromJSON(jsoncode_definition_1);
	cJSON* jsoncode_definition_2 = code_definition_convertToJSON(code_definition_2);
	printf("repeating code_definition:\n%s\n", cJSON_Print(jsoncode_definition_2));
}

int main() {
  test_code_definition(1);
  test_code_definition(0);

  printf("Hello world \n");
  return 0;
}

#endif // code_definition_MAIN
#endif // code_definition_TEST
