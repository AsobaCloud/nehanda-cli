#ifndef maintenance_reconcile_request_TEST
#define maintenance_reconcile_request_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define maintenance_reconcile_request_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/maintenance_reconcile_request.h"
maintenance_reconcile_request_t* instantiate_maintenance_reconcile_request(int include_optional);



maintenance_reconcile_request_t* instantiate_maintenance_reconcile_request(int include_optional) {
  maintenance_reconcile_request_t* maintenance_reconcile_request = NULL;
  if (include_optional) {
    maintenance_reconcile_request = maintenance_reconcile_request_create(
      1
    );
  } else {
    maintenance_reconcile_request = maintenance_reconcile_request_create(
      1
    );
  }

  return maintenance_reconcile_request;
}


#ifdef maintenance_reconcile_request_MAIN

void test_maintenance_reconcile_request(int include_optional) {
    maintenance_reconcile_request_t* maintenance_reconcile_request_1 = instantiate_maintenance_reconcile_request(include_optional);

	cJSON* jsonmaintenance_reconcile_request_1 = maintenance_reconcile_request_convertToJSON(maintenance_reconcile_request_1);
	printf("maintenance_reconcile_request :\n%s\n", cJSON_Print(jsonmaintenance_reconcile_request_1));
	maintenance_reconcile_request_t* maintenance_reconcile_request_2 = maintenance_reconcile_request_parseFromJSON(jsonmaintenance_reconcile_request_1);
	cJSON* jsonmaintenance_reconcile_request_2 = maintenance_reconcile_request_convertToJSON(maintenance_reconcile_request_2);
	printf("repeating maintenance_reconcile_request:\n%s\n", cJSON_Print(jsonmaintenance_reconcile_request_2));
}

int main() {
  test_maintenance_reconcile_request(1);
  test_maintenance_reconcile_request(0);

  printf("Hello world \n");
  return 0;
}

#endif // maintenance_reconcile_request_MAIN
#endif // maintenance_reconcile_request_TEST
