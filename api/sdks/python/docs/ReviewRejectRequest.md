# ReviewRejectRequest


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**reason** | **str** |  | 

## Example

```python
from aimee_kb.models.review_reject_request import ReviewRejectRequest

# TODO update the JSON string below
json = "{}"
# create an instance of ReviewRejectRequest from a JSON string
review_reject_request_instance = ReviewRejectRequest.from_json(json)
# print the JSON string representation of the object
print(ReviewRejectRequest.to_json())

# convert the object into a dict
review_reject_request_dict = review_reject_request_instance.to_dict()
# create an instance of ReviewRejectRequest from a dict
review_reject_request_from_dict = ReviewRejectRequest.from_dict(review_reject_request_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


