#ifndef maintenance_repair_request_TEST
#define maintenance_repair_request_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define maintenance_repair_request_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/maintenance_repair_request.h"
maintenance_repair_request_t* instantiate_maintenance_repair_request(int include_optional);



maintenance_repair_request_t* instantiate_maintenance_repair_request(int include_optional) {
  maintenance_repair_request_t* maintenance_repair_request = NULL;
  if (include_optional) {
    maintenance_repair_request = maintenance_repair_request_create(
      "0",
      "0",
      "0"
    );
  } else {
    maintenance_repair_request = maintenance_repair_request_create(
      "0",
      "0",
      "0"
    );
  }

  return maintenance_repair_request;
}


#ifdef maintenance_repair_request_MAIN

void test_maintenance_repair_request(int include_optional) {
    maintenance_repair_request_t* maintenance_repair_request_1 = instantiate_maintenance_repair_request(include_optional);

	cJSON* jsonmaintenance_repair_request_1 = maintenance_repair_request_convertToJSON(maintenance_repair_request_1);
	printf("maintenance_repair_request :\n%s\n", cJSON_Print(jsonmaintenance_repair_request_1));
	maintenance_repair_request_t* maintenance_repair_request_2 = maintenance_repair_request_parseFromJSON(jsonmaintenance_repair_request_1);
	cJSON* jsonmaintenance_repair_request_2 = maintenance_repair_request_convertToJSON(maintenance_repair_request_2);
	printf("repeating maintenance_repair_request:\n%s\n", cJSON_Print(jsonmaintenance_repair_request_2));
}

int main() {
  test_maintenance_repair_request(1);
  test_maintenance_repair_request(0);

  printf("Hello world \n");
  return 0;
}

#endif // maintenance_repair_request_MAIN
#endif // maintenance_repair_request_TEST
