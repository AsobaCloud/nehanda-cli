# DrainRequest


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**embedding_command** | **str** |  | [optional] 
**timeout** | **int** |  | [optional] [default to 0]

## Example

```python
from aimee_kb.models.drain_request import DrainRequest

# TODO update the JSON string below
json = "{}"
# create an instance of DrainRequest from a JSON string
drain_request_instance = DrainRequest.from_json(json)
# print the JSON string representation of the object
print(DrainRequest.to_json())

# convert the object into a dict
drain_request_dict = drain_request_instance.to_dict()
# create an instance of DrainRequest from a dict
drain_request_from_dict = DrainRequest.from_dict(drain_request_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


