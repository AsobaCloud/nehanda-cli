# ReviewQueueResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**docs** | [**List[DocMetadataResponse]**](DocMetadataResponse.md) |  | [optional] 
**next_cursor** | **int** |  | [optional] 

## Example

```python
from aimee_kb.models.review_queue_response import ReviewQueueResponse

# TODO update the JSON string below
json = "{}"
# create an instance of ReviewQueueResponse from a JSON string
review_queue_response_instance = ReviewQueueResponse.from_json(json)
# print the JSON string representation of the object
print(ReviewQueueResponse.to_json())

# convert the object into a dict
review_queue_response_dict = review_queue_response_instance.to_dict()
# create an instance of ReviewQueueResponse from a dict
review_queue_response_from_dict = ReviewQueueResponse.from_dict(review_queue_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


