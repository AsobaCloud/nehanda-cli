#ifndef maintenance_clear_request_TEST
#define maintenance_clear_request_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define maintenance_clear_request_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/maintenance_clear_request.h"
maintenance_clear_request_t* instantiate_maintenance_clear_request(int include_optional);



maintenance_clear_request_t* instantiate_maintenance_clear_request(int include_optional) {
  maintenance_clear_request_t* maintenance_clear_request = NULL;
  if (include_optional) {
    maintenance_clear_request = maintenance_clear_request_create(
      "0"
    );
  } else {
    maintenance_clear_request = maintenance_clear_request_create(
      "0"
    );
  }

  return maintenance_clear_request;
}


#ifdef maintenance_clear_request_MAIN

void test_maintenance_clear_request(int include_optional) {
    maintenance_clear_request_t* maintenance_clear_request_1 = instantiate_maintenance_clear_request(include_optional);

	cJSON* jsonmaintenance_clear_request_1 = maintenance_clear_request_convertToJSON(maintenance_clear_request_1);
	printf("maintenance_clear_request :\n%s\n", cJSON_Print(jsonmaintenance_clear_request_1));
	maintenance_clear_request_t* maintenance_clear_request_2 = maintenance_clear_request_parseFromJSON(jsonmaintenance_clear_request_1);
	cJSON* jsonmaintenance_clear_request_2 = maintenance_clear_request_convertToJSON(maintenance_clear_request_2);
	printf("repeating maintenance_clear_request:\n%s\n", cJSON_Print(jsonmaintenance_clear_request_2));
}

int main() {
  test_maintenance_clear_request(1);
  test_maintenance_clear_request(0);

  printf("Hello world \n");
  return 0;
}

#endif // maintenance_clear_request_MAIN
#endif // maintenance_clear_request_TEST
