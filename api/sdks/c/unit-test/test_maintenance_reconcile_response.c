#ifndef maintenance_reconcile_response_TEST
#define maintenance_reconcile_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define maintenance_reconcile_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/maintenance_reconcile_response.h"
maintenance_reconcile_response_t* instantiate_maintenance_reconcile_response(int include_optional);

#include "test_maintenance_reconcile_response_memory.c"
#include "test_maintenance_reconcile_response_memory.c"


maintenance_reconcile_response_t* instantiate_maintenance_reconcile_response(int include_optional) {
  maintenance_reconcile_response_t* maintenance_reconcile_response = NULL;
  if (include_optional) {
    maintenance_reconcile_response = maintenance_reconcile_response_create(
      "ok",
      56,
      1,
       // false, not to have infinite recursion
      instantiate_maintenance_reconcile_response_memory(0),
       // false, not to have infinite recursion
      instantiate_maintenance_reconcile_response_memory(0)
    );
  } else {
    maintenance_reconcile_response = maintenance_reconcile_response_create(
      "ok",
      56,
      1,
      NULL,
      NULL
    );
  }

  return maintenance_reconcile_response;
}


#ifdef maintenance_reconcile_response_MAIN

void test_maintenance_reconcile_response(int include_optional) {
    maintenance_reconcile_response_t* maintenance_reconcile_response_1 = instantiate_maintenance_reconcile_response(include_optional);

	cJSON* jsonmaintenance_reconcile_response_1 = maintenance_reconcile_response_convertToJSON(maintenance_reconcile_response_1);
	printf("maintenance_reconcile_response :\n%s\n", cJSON_Print(jsonmaintenance_reconcile_response_1));
	maintenance_reconcile_response_t* maintenance_reconcile_response_2 = maintenance_reconcile_response_parseFromJSON(jsonmaintenance_reconcile_response_1);
	cJSON* jsonmaintenance_reconcile_response_2 = maintenance_reconcile_response_convertToJSON(maintenance_reconcile_response_2);
	printf("repeating maintenance_reconcile_response:\n%s\n", cJSON_Print(jsonmaintenance_reconcile_response_2));
}

int main() {
  test_maintenance_reconcile_response(1);
  test_maintenance_reconcile_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // maintenance_reconcile_response_MAIN
#endif // maintenance_reconcile_response_TEST
