#ifndef maintenance_clear_response_TEST
#define maintenance_clear_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define maintenance_clear_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/maintenance_clear_response.h"
maintenance_clear_response_t* instantiate_maintenance_clear_response(int include_optional);



maintenance_clear_response_t* instantiate_maintenance_clear_response(int include_optional) {
  maintenance_clear_response_t* maintenance_clear_response = NULL;
  if (include_optional) {
    maintenance_clear_response = maintenance_clear_response_create(
      "ok",
      "0",
      56
    );
  } else {
    maintenance_clear_response = maintenance_clear_response_create(
      "ok",
      "0",
      56
    );
  }

  return maintenance_clear_response;
}


#ifdef maintenance_clear_response_MAIN

void test_maintenance_clear_response(int include_optional) {
    maintenance_clear_response_t* maintenance_clear_response_1 = instantiate_maintenance_clear_response(include_optional);

	cJSON* jsonmaintenance_clear_response_1 = maintenance_clear_response_convertToJSON(maintenance_clear_response_1);
	printf("maintenance_clear_response :\n%s\n", cJSON_Print(jsonmaintenance_clear_response_1));
	maintenance_clear_response_t* maintenance_clear_response_2 = maintenance_clear_response_parseFromJSON(jsonmaintenance_clear_response_1);
	cJSON* jsonmaintenance_clear_response_2 = maintenance_clear_response_convertToJSON(maintenance_clear_response_2);
	printf("repeating maintenance_clear_response:\n%s\n", cJSON_Print(jsonmaintenance_clear_response_2));
}

int main() {
  test_maintenance_clear_response(1);
  test_maintenance_clear_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // maintenance_clear_response_MAIN
#endif // maintenance_clear_response_TEST
