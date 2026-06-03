# MaintenanceClearResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**status** | **str** |  | [optional] 
**project** | **str** |  | [optional] 
**chunks_deleted** | **int** |  | [optional] 

## Example

```python
from aimee_kb.models.maintenance_clear_response import MaintenanceClearResponse

# TODO update the JSON string below
json = "{}"
# create an instance of MaintenanceClearResponse from a JSON string
maintenance_clear_response_instance = MaintenanceClearResponse.from_json(json)
# print the JSON string representation of the object
print(MaintenanceClearResponse.to_json())

# convert the object into a dict
maintenance_clear_response_dict = maintenance_clear_response_instance.to_dict()
# create an instance of MaintenanceClearResponse from a dict
maintenance_clear_response_from_dict = MaintenanceClearResponse.from_dict(maintenance_clear_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


