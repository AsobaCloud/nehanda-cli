# WorkersResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**status** | **str** |  | [optional] 
**configured** | **int** |  | [optional] 
**slots** | **List[object]** |  | [optional] 
**threads** | **List[object]** |  | [optional] 
**background** | **List[object]** |  | [optional] 

## Example

```python
from aimee_kb.models.workers_response import WorkersResponse

# TODO update the JSON string below
json = "{}"
# create an instance of WorkersResponse from a JSON string
workers_response_instance = WorkersResponse.from_json(json)
# print the JSON string representation of the object
print(WorkersResponse.to_json())

# convert the object into a dict
workers_response_dict = workers_response_instance.to_dict()
# create an instance of WorkersResponse from a dict
workers_response_from_dict = WorkersResponse.from_dict(workers_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


