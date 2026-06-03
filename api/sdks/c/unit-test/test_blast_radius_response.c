#ifndef blast_radius_response_TEST
#define blast_radius_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define blast_radius_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/blast_radius_response.h"
blast_radius_response_t* instantiate_blast_radius_response(int include_optional);



blast_radius_response_t* instantiate_blast_radius_response(int include_optional) {
  blast_radius_response_t* blast_radius_response = NULL;
  if (include_optional) {
    blast_radius_response = blast_radius_response_create(
      "0",
      list_createList(),
      56,
      list_createList(),
      56
    );
  } else {
    blast_radius_response = blast_radius_response_create(
      "0",
      list_createList(),
      56,
      list_createList(),
      56
    );
  }

  return blast_radius_response;
}


#ifdef blast_radius_response_MAIN

void test_blast_radius_response(int include_optional) {
    blast_radius_response_t* blast_radius_response_1 = instantiate_blast_radius_response(include_optional);

	cJSON* jsonblast_radius_response_1 = blast_radius_response_convertToJSON(blast_radius_response_1);
	printf("blast_radius_response :\n%s\n", cJSON_Print(jsonblast_radius_response_1));
	blast_radius_response_t* blast_radius_response_2 = blast_radius_response_parseFromJSON(jsonblast_radius_response_1);
	cJSON* jsonblast_radius_response_2 = blast_radius_response_convertToJSON(blast_radius_response_2);
	printf("repeating blast_radius_response:\n%s\n", cJSON_Print(jsonblast_radius_response_2));
}

int main() {
  test_blast_radius_response(1);
  test_blast_radius_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // blast_radius_response_MAIN
#endif // blast_radius_response_TEST
