#ifndef maintenance_reconcile_response_memory_TEST
#define maintenance_reconcile_response_memory_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define maintenance_reconcile_response_memory_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/maintenance_reconcile_response_memory.h"
maintenance_reconcile_response_memory_t* instantiate_maintenance_reconcile_response_memory(int include_optional);



maintenance_reconcile_response_memory_t* instantiate_maintenance_reconcile_response_memory(int include_optional) {
  maintenance_reconcile_response_memory_t* maintenance_reconcile_response_memory = NULL;
  if (include_optional) {
    maintenance_reconcile_response_memory = maintenance_reconcile_response_memory_create(
      56,
      56
    );
  } else {
    maintenance_reconcile_response_memory = maintenance_reconcile_response_memory_create(
      56,
      56
    );
  }

  return maintenance_reconcile_response_memory;
}


#ifdef maintenance_reconcile_response_memory_MAIN

void test_maintenance_reconcile_response_memory(int include_optional) {
    maintenance_reconcile_response_memory_t* maintenance_reconcile_response_memory_1 = instantiate_maintenance_reconcile_response_memory(include_optional);

	cJSON* jsonmaintenance_reconcile_response_memory_1 = maintenance_reconcile_response_memory_convertToJSON(maintenance_reconcile_response_memory_1);
	printf("maintenance_reconcile_response_memory :\n%s\n", cJSON_Print(jsonmaintenance_reconcile_response_memory_1));
	maintenance_reconcile_response_memory_t* maintenance_reconcile_response_memory_2 = maintenance_reconcile_response_memory_parseFromJSON(jsonmaintenance_reconcile_response_memory_1);
	cJSON* jsonmaintenance_reconcile_response_memory_2 = maintenance_reconcile_response_memory_convertToJSON(maintenance_reconcile_response_memory_2);
	printf("repeating maintenance_reconcile_response_memory:\n%s\n", cJSON_Print(jsonmaintenance_reconcile_response_memory_2));
}

int main() {
  test_maintenance_reconcile_response_memory(1);
  test_maintenance_reconcile_response_memory(0);

  printf("Hello world \n");
  return 0;
}

#endif // maintenance_reconcile_response_memory_MAIN
#endif // maintenance_reconcile_response_memory_TEST
