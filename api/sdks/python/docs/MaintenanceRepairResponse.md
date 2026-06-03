# MaintenanceRepairResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**status** | **str** |  | [optional] 
**project** | **str** |  | [optional] 
**files_scanned** | **int** |  | [optional] 
**files_indexed** | **int** |  | [optional] 
**files_skipped** | **int** |  | [optional] 
**files_removed** | **int** |  | [optional] 
**chunks_added** | **int** |  | [optional] 
**chunks_removed** | **int** |  | [optional] 
**embeddings_added** | **int** |  | [optional] 

## Example

```python
from aimee_kb.models.maintenance_repair_response import MaintenanceRepairResponse

# TODO update the JSON string below
json = "{}"
# create an instance of MaintenanceRepairResponse from a JSON string
maintenance_repair_response_instance = MaintenanceRepairResponse.from_json(json)
# print the JSON string representation of the object
print(MaintenanceRepairResponse.to_json())

# convert the object into a dict
maintenance_repair_response_dict = maintenance_repair_response_instance.to_dict()
# create an instance of MaintenanceRepairResponse from a dict
maintenance_repair_response_from_dict = MaintenanceRepairResponse.from_dict(maintenance_repair_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


