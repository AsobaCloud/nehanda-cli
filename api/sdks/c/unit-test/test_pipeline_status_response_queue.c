#ifndef pipeline_status_response_queue_TEST
#define pipeline_status_response_queue_TEST

// the following is to include only the main from the first c file
#ifndef TEST_MAIN
#define TEST_MAIN
#define pipeline_status_response_queue_MAIN
#endif // TEST_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../external/cJSON.h"

#include "../model/pipeline_status_response_queue.h"
pipeline_status_response_queue_t* instantiate_pipeline_status_response_queue(int include_optional);



pipeline_status_response_queue_t* instantiate_pipeline_status_response_queue(int include_optional) {
  pipeline_status_response_queue_t* pipeline_status_response_queue = NULL;
  if (include_optional) {
    pipeline_status_response_queue = pipeline_status_response_queue_create(
      56,
      56,
      56,
      56,
      56
    );
  } else {
    pipeline_status_response_queue = pipeline_status_response_queue_create(
      56,
      56,
      56,
      56,
      56
    );
  }

  return pipeline_status_response_queue;
}


#ifdef pipeline_status_response_queue_MAIN

void test_pipeline_status_response_queue(int include_optional) {
    pipeline_status_response_queue_t* pipeline_status_response_queue_1 = instantiate_pipeline_status_response_queue(include_optional);

	cJSON* jsonpipeline_status_response_queue_1 = pipeline_status_response_queue_convertToJSON(pipeline_status_response_queue_1);
	printf("pipeline_status_response_queue :\n%s\n", cJSON_Print(jsonpipeline_status_response_queue_1));
	pipeline_status_response_queue_t* pipeline_status_response_queue_2 = pipeline_status_response_queue_parseFromJSON(jsonpipeline_status_response_queue_1);
	cJSON* jsonpipeline_status_response_queue_2 = pipeline_status_response_queue_convertToJSON(pipeline_status_response_queue_2);
	printf("repeating pipeline_status_response_queue:\n%s\n", cJSON_Print(jsonpipeline_status_response_queue_2));
}

int main() {
  test_pipeline_status_response_queue(1);
  test_pipeline_status_response_queue(0);

  printf("Hello world \n");
  return 0;
}

#endif // pipeline_status_response_queue_MAIN
#endif // pipeline_status_response_queue_TEST
