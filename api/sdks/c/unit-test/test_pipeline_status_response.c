#ifndef pipeline_status_response_TEST
#define pipeline_status_response_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define pipeline_status_response_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/pipeline_status_response.h"
pipeline_status_response_t* instantiate_pipeline_status_response(int include_optional);

#include "test_pipeline_status_response_queue.c"


pipeline_status_response_t* instantiate_pipeline_status_response(int include_optional) {
  pipeline_status_response_t* pipeline_status_response = NULL;
  if (include_optional) {
    pipeline_status_response = pipeline_status_response_create(
      aimee_kb_api_pipeline_status_response_STATE_idle,
      56,
      list_createList(),
       // false, not to have infinite recursion
      instantiate_pipeline_status_response_queue(0)
    );
  } else {
    pipeline_status_response = pipeline_status_response_create(
      aimee_kb_api_pipeline_status_response_STATE_idle,
      56,
      list_createList(),
      NULL
    );
  }

  return pipeline_status_response;
}


#ifdef pipeline_status_response_MAIN

void test_pipeline_status_response(int include_optional) {
    pipeline_status_response_t* pipeline_status_response_1 = instantiate_pipeline_status_response(include_optional);

	cJSON* jsonpipeline_status_response_1 = pipeline_status_response_convertToJSON(pipeline_status_response_1);
	printf("pipeline_status_response :\n%s\n", cJSON_Print(jsonpipeline_status_response_1));
	pipeline_status_response_t* pipeline_status_response_2 = pipeline_status_response_parseFromJSON(jsonpipeline_status_response_1);
	cJSON* jsonpipeline_status_response_2 = pipeline_status_response_convertToJSON(pipeline_status_response_2);
	printf("repeating pipeline_status_response:\n%s\n", cJSON_Print(jsonpipeline_status_response_2));
}

int main() {
  test_pipeline_status_response(1);
  test_pipeline_status_response(0);

  printf("Hello world \n");
  return 0;
}

#endif // pipeline_status_response_MAIN
#endif // pipeline_status_response_TEST
