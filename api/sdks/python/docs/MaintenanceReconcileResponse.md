# MaintenanceReconcileResponse


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**status** | **str** |  | [optional] 
**rc** | **int** |  | [optional] 
**dry_run** | **bool** |  | [optional] 
**memory** | [**MaintenanceReconcileResponseMemory**](MaintenanceReconcileResponseMemory.md) |  | [optional] 
**kb** | [**MaintenanceReconcileResponseMemory**](MaintenanceReconcileResponseMemory.md) |  | [optional] 

## Example

```python
from aimee_kb.models.maintenance_reconcile_response import MaintenanceReconcileResponse

# TODO update the JSON string below
json = "{}"
# create an instance of MaintenanceReconcileResponse from a JSON string
maintenance_reconcile_response_instance = MaintenanceReconcileResponse.from_json(json)
# print the JSON string representation of the object
print(MaintenanceReconcileResponse.to_json())

# convert the object into a dict
maintenance_reconcile_response_dict = maintenance_reconcile_response_instance.to_dict()
# create an instance of MaintenanceReconcileResponse from a dict
maintenance_reconcile_response_from_dict = MaintenanceReconcileResponse.from_dict(maintenance_reconcile_response_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


