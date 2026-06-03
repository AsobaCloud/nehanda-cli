#ifndef entity_search_response_entities_inner_TEST
#define entity_search_response_entities_inner_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define entity_search_response_entities_inner_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/entity_search_response_entities_inner.h"
entity_search_response_entities_inner_t* instantiate_entity_search_response_entities_inner(int include_optional);



entity_search_response_entities_inner_t* instantiate_entity_search_response_entities_inner(int include_optional) {
  entity_search_response_entities_inner_t* entity_search_response_entities_inner = NULL;
  if (include_optional) {
    entity_search_response_entities_inner = entity_search_response_entities_inner_create(
      "0",
      "0",
      "0",
      1.337
    );
  } else {
    entity_search_response_entities_inner = entity_search_response_entities_inner_create(
      "0",
      "0",
      "0",
      1.337
    );
  }

  return entity_search_response_entities_inner;
}


#ifdef entity_search_response_entities_inner_MAIN

void test_entity_search_response_entities_inner(int include_optional) {
    entity_search_response_entities_inner_t* entity_search_response_entities_inner_1 = instantiate_entity_search_response_entities_inner(include_optional);

	cJSON* jsonentity_search_response_entities_inner_1 = entity_search_response_entities_inner_convertToJSON(entity_search_response_entities_inner_1);
	printf("entity_search_response_entities_inner :\n%s\n", cJSON_Print(jsonentity_search_response_entities_inner_1));
	entity_search_response_entities_inner_t* entity_search_response_entities_inner_2 = entity_search_response_entities_inner_parseFromJSON(jsonentity_search_response_entities_inner_1);
	cJSON* jsonentity_search_response_entities_inner_2 = entity_search_response_entities_inner_convertToJSON(entity_search_response_entities_inner_2);
	printf("repeating entity_search_response_entities_inner:\n%s\n", cJSON_Print(jsonentity_search_response_entities_inner_2));
}

int main() {
  test_entity_search_response_entities_inner(1);
  test_entity_search_response_entities_inner(0);

  printf("Hello world \n");
  return 0;
}

#endif // entity_search_response_entities_inner_MAIN
#endif // entity_search_response_entities_inner_TEST
