# DrainResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**state** | **str** |  | [optional] 
**processed** | **int** |  | [optional] 
**pending** | **int** |  | [optional] 
**running** | **int** |  | [optional] 
**done** | **int** |  | [optional] 
**failed** | **int** |  | [optional] 
**total** | **int** |  | [optional] 

## Example

```python
from aimee_kb.models.drain_response import DrainResponse

# TODO update the JSON string below
json = "{}"
# create an instance of DrainResponse from a JSON string
drain_response_instance = DrainResponse.from_json(json)
# print the JSON string representation of the object
print(DrainResponse.to_json())

# convert the object into a dict
drain_response_dict = drain_response_instance.to_dict()
# create an instance of DrainResponse from a dict
drain_response_from_dict = DrainResponse.from_dict(drain_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


