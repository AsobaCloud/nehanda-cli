# MaintenanceRepairRequest


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**path** | **str** |  | 
**project** | **str** |  | 
**embedding_command** | **str** |  | [optional] 

## Example

```python
from aimee_kb.models.maintenance_repair_request import MaintenanceRepairRequest

# TODO update the JSON string below
json = "{}"
# create an instance of MaintenanceRepairRequest from a JSON string
maintenance_repair_request_instance = MaintenanceRepairRequest.from_json(json)
# print the JSON string representation of the object
print(MaintenanceRepairRequest.to_json())

# convert the object into a dict
maintenance_repair_request_dict = maintenance_repair_request_instance.to_dict()
# create an instance of MaintenanceRepairRequest from a dict
maintenance_repair_request_from_dict = MaintenanceRepairRequest.from_dict(maintenance_repair_request_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


