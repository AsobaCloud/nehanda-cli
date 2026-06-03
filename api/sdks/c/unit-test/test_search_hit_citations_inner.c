#ifndef search_hit_citations_inner_TEST
#define search_hit_citations_inner_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define search_hit_citations_inner_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/search_hit_citations_inner.h"
search_hit_citations_inner_t* instantiate_search_hit_citations_inner(int include_optional);



search_hit_citations_inner_t* instantiate_search_hit_citations_inner(int include_optional) {
  search_hit_citations_inner_t* search_hit_citations_inner = NULL;
  if (include_optional) {
    search_hit_citations_inner = search_hit_citations_inner_create(
      "0",
      "0"
    );
  } else {
    search_hit_citations_inner = search_hit_citations_inner_create(
      "0",
      "0"
    );
  }

  return search_hit_citations_inner;
}


#ifdef search_hit_citations_inner_MAIN

void test_search_hit_citations_inner(int include_optional) {
    search_hit_citations_inner_t* search_hit_citations_inner_1 = instantiate_search_hit_citations_inner(include_optional);

	cJSON* jsonsearch_hit_citations_inner_1 = search_hit_citations_inner_convertToJSON(search_hit_citations_inner_1);
	printf("search_hit_citations_inner :\n%s\n", cJSON_Print(jsonsearch_hit_citations_inner_1));
	search_hit_citations_inner_t* search_hit_citations_inner_2 = search_hit_citations_inner_parseFromJSON(jsonsearch_hit_citations_inner_1);
	cJSON* jsonsearch_hit_citations_inner_2 = search_hit_citations_inner_convertToJSON(search_hit_citations_inner_2);
	printf("repeating search_hit_citations_inner:\n%s\n", cJSON_Print(jsonsearch_hit_citations_inner_2));
}

int main() {
  test_search_hit_citations_inner(1);
  test_search_hit_citations_inner(0);

  printf("Hello world \n");
  return 0;
}

#endif // search_hit_citations_inner_MAIN
#endif // search_hit_citations_inner_TEST
