#ifndef maintenance_repair_response_TEST
#define maintenance_repair_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define maintenance_repair_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/maintenance_repair_response.h"
maintenance_repair_response_t* instantiate_maintenance_repair_response(int include_optional);



maintenance_repair_response_t* instantiate_maintenance_repair_response(int include_optional) {
  maintenance_repair_response_t* maintenance_repair_response = NULL;
  if (include_optional) {
    maintenance_repair_response = maintenance_repair_response_create(
      "ok",
      "0",
      56,
      56,
      56,
      56,
      56,
      56,
      56
    );
  } else {
    maintenance_repair_response = maintenance_repair_response_create(
      "ok",
      "0",
      56,
      56,
      56,
      56,
      56,
      56,
      56
    );
  }

  return maintenance_repair_response;
}


#ifdef maintenance_repair_response_MAIN

void test_maintenance_repair_response(int include_optional) {
    maintenance_repair_response_t* maintenance_repair_response_1 = instantiate_maintenance_repair_response(include_optional);

	cJSON* jsonmaintenance_repair_response_1 = maintenance_repair_response_convertToJSON(maintenance_repair_response_1);
	printf("maintenance_repair_response :\n%s\n", cJSON_Print(jsonmaintenance_repair_response_1));
	maintenance_repair_response_t* maintenance_repair_response_2 = maintenance_repair_response_parseFromJSON(jsonmaintenance_repair_response_1);
	cJSON* jsonmaintenance_repair_response_2 = maintenance_repair_response_convertToJSON(maintenance_repair_response_2);
	printf("repeating maintenance_repair_response:\n%s\n", cJSON_Print(jsonmaintenance_repair_response_2));
}

int main() {
  test_maintenance_repair_response(1);
  test_maintenance_repair_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // maintenance_repair_response_MAIN
#endif // maintenance_repair_response_TEST
