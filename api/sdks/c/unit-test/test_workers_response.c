#ifndef workers_response_TEST
#define workers_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define workers_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/workers_response.h"
workers_response_t* instantiate_workers_response(int include_optional);



workers_response_t* instantiate_workers_response(int include_optional) {
  workers_response_t* workers_response = NULL;
  if (include_optional) {
    workers_response = workers_response_create(
      "ok",
      56,
      list_createList(),
      list_createList(),
      list_createList()
    );
  } else {
    workers_response = workers_response_create(
      "ok",
      56,
      list_createList(),
      list_createList(),
      list_createList()
    );
  }

  return workers_response;
}


#ifdef workers_response_MAIN

void test_workers_response(int include_optional) {
    workers_response_t* workers_response_1 = instantiate_workers_response(include_optional);

	cJSON* jsonworkers_response_1 = workers_response_convertToJSON(workers_response_1);
	printf("workers_response :\n%s\n", cJSON_Print(jsonworkers_response_1));
	workers_response_t* workers_response_2 = workers_response_parseFromJSON(jsonworkers_response_1);
	cJSON* jsonworkers_response_2 = workers_response_convertToJSON(workers_response_2);
	printf("repeating workers_response:\n%s\n", cJSON_Print(jsonworkers_response_2));
}

int main() {
  test_workers_response(1);
  test_workers_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // workers_response_MAIN
#endif // workers_response_TEST
